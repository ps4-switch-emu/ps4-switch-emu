// Host-side test for the PS4 fastmem arena: the real class with the kernel calls replaced by a
// simulated address space of 16 KiB pages. Random guest Map/Unmap/Protect sequences; after each,
// every block must be mapped exactly when its four guest pages line up (and with the right
// backing page and protection), and reserved-but-unmapped otherwise.
#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <random>
#include <unordered_map>
#include <utility>
#include <sys/stat.h>
#include <sys/types.h>

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
enum class MemoryPermission : u32 { Read = 1, Write = 2, ReadWrite = 3, Execute = 4 };
#define ASSERT_MSG(c, ...) assert(c)
#define LOG_INFO(...) ((void)0)
static void (*ps4_boot_trace)(const char*) = nullptr;

// --- simulated kernel: one entry per 16 KiB page of the process address space -----------------
struct Page {
    enum Kind { Free, Reserved, Mapped } kind = Free;
    off_t phys = 0;
    int prot = 0;
};
static std::map<uint64_t, Page> g_pages; // address -> page
static constexpr uint64_t kPage = 0x4000;
static uint64_t g_next_free = 0x100000000000ull;
static off_t g_phys_used = 0;

extern "C" {
size_t sceKernelGetDirectMemorySize() { return size_t(4608) << 20; }
int32_t sceKernelAvailableDirectMemorySize(off_t, off_t, size_t, off_t* at, size_t* size) {
    *at = 0;
    *size = sceKernelGetDirectMemorySize() - g_phys_used;
    return 0;
}
int32_t sceKernelEnableDmemAliasing() { return 0; }
int32_t sceKernelAllocateDirectMemory(off_t, off_t, size_t len, size_t, int32_t, off_t* phys) {
    *phys = g_phys_used;
    g_phys_used += len;
    return 0;
}
int32_t sceKernelReleaseDirectMemory(off_t, size_t) { return 0; }
int32_t sceKernelMapDirectMemory(void** addr, size_t len, int32_t prot, int32_t flags, off_t phys, size_t) {
    assert(len % kPage == 0);
    uint64_t a = uint64_t(*addr);
    if (!(flags & 0x10)) { // not fixed: backing, put it anywhere
        a = g_next_free;
        g_next_free += len + kPage;
        *addr = (void*)a;
        return 0; // backing isn't tracked page by page
    }
    for (uint64_t o = 0; o < len; o += kPage) {
        Page& p = g_pages[a + o];
        assert(p.kind == Page::Free && "MAP_FIXED over something that wasn't unmapped first");
        p = {Page::Mapped, phys + off_t(o), prot};
    }
    return 0;
}
int32_t sceKernelReserveVirtualRange(void** addr, size_t len, int32_t flags, size_t) {
    uint64_t a = uint64_t(*addr);
    if (!(flags & 0x10)) {
        a = g_next_free;
        g_next_free += len + kPage;
        *addr = (void*)a;
        // The arena is huge: record it lazily (absent pages inside it count as Reserved).
        return 0;
    }
    for (uint64_t o = 0; o < len; o += kPage) {
        Page& p = g_pages[a + o];
        assert(p.kind == Page::Free);
        p = {Page::Reserved, 0, 0};
    }
    return 0;
}
int32_t sceKernelMunmap(void* addr, size_t len) {
    for (uint64_t o = 0; o < len; o += kPage)
        g_pages[uint64_t(addr) + o] = {Page::Free, 0, 0};
    return 0;
}
int32_t sceKernelMprotect(const void* addr, size_t len, int32_t prot) {
    for (uint64_t o = 0; o < len; o += kPage) {
        Page& p = g_pages[uint64_t(addr) + o];
        assert(p.kind == Page::Mapped);
        p.prot = prot;
    }
    return 0;
}
}

#include "arena_body.inc"

// --- reference model ---------------------------------------------------------------------------
struct GuestPage { uint32_t host = 0; uint8_t perm = 0; }; // host page index + 1, 0 = unmapped
static std::map<uint64_t, GuestPage> g_guest;                // guest 4 KiB page index -> state

static void Check(PS4Arena& arena, uint64_t first_block, uint64_t last_block) {
    const uint64_t base = uint64_t(arena.virtual_base);
    for (uint64_t b = first_block; b <= last_block; b++) {
        GuestPage g[4];
        for (int i = 0; i < 4; i++) {
            auto it = g_guest.find(b * 4 + i);
            if (it != g_guest.end()) g[i] = it->second;
        }
        bool want = g[0].host != 0 && (g[0].host - 1) % 4 == 0 && (g[0].perm & 1);
        for (int i = 1; i < 4 && want; i++)
            want = g[i].host == g[0].host + i && g[i].perm == g[0].perm;
        auto pit = g_pages.find(base + b * kPage);
        const Page p = pit == g_pages.end() ? Page{Page::Reserved, 0, 0} : pit->second;
        if (want) {
            const int prot = (g[0].perm & 2) ? 3 : 1;
            const off_t phys = off_t(g[0].host - 1) * 0x1000; // arena phys base is 0 (first allocation)
            if (p.kind != Page::Mapped || p.phys != phys || p.prot != prot) {
                fprintf(stderr, "block %llu: want mapped phys %#llx prot %d, have kind %d phys %#llx prot %d\n",
                        (unsigned long long)b, (long long)phys, prot, p.kind, (long long)p.phys, p.prot);
                abort();
            }
        } else if (p.kind != Page::Reserved) {
            fprintf(stderr, "block %llu: want reserved, have kind %d\n", (unsigned long long)b, p.kind);
            abort();
        }
    }
}

int main() {
    PS4Arena arena(size_t(4) << 30, size_t(1) << 39);
    assert(arena.virtual_base && arena.backing_base);
    std::mt19937_64 rng(1234);
    const uint64_t blocks = 256; // test window: 256 blocks = 4 MiB of guest space
    const auto r = [&](uint64_t n) { return rng() % n; };
    for (int step = 0; step < 200000; step++) {
        const uint64_t page = r(blocks * 4), count = 1 + r(std::min<uint64_t>(64, blocks * 4 - page));
        const int op = int(r(3));
        const uint8_t perm = uint8_t(r(4)); // none, R, W, RW
        if (op == 0) {
            // Map: mostly aligned & contiguous host pages, sometimes misaligned.
            const uint32_t host = uint32_t(r(4096)) & (r(4) ? ~3u : ~0u);
            arena.Map(page * 0x1000, size_t(host) * 0x1000, count * 0x1000, MemoryPermission(perm));
            for (uint64_t i = 0; i < count; i++) g_guest[page + i] = {uint32_t(host + i + 1), perm};
        } else if (op == 1) {
            arena.Unmap(page * 0x1000, count * 0x1000);
            for (uint64_t i = 0; i < count; i++) g_guest.erase(page + i);
        } else {
            arena.Protect(page * 0x1000, count * 0x1000, MemoryPermission(perm));
            for (uint64_t i = 0; i < count; i++)
                if (auto it = g_guest.find(page + i); it != g_guest.end()) it->second.perm = perm;
        }
        Check(arena, 0, blocks - 1);
    }
    arena.Report("end");
    printf("fastmem arena test: 200000 random operations OK\n");
    return 0;
}
