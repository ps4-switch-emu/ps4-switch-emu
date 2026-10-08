// PS4 Switch Emu bring-up benchmark: a Switch homebrew that needs no graphics.
//
// Three tests with a fixed amount of work each; progress lines go to sdmc:/cpu-benchmark.txt
// (on the PS4: /data/switch-emu/sdmc/cpu-benchmark.txt, shown live on the PS4 status screen).
// Real durations come from the host's timestamps of those lines; the emulated time is printed
// for reference. Run the same .nro in Eden on a PC for a baseline.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <switch.h>

#define TICKS_PER_SEC 19200000ull

#define OUT_PATH "sdmc:/cpu-benchmark.txt"

static void report(const char* fmt, ...) {
    char line[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    // Open, append, close for every line: Eden only writes a guest file out to the host when
    // it's flushed by a later write or closed, and the host timestamps each line as it appears.
    FILE* out = fopen(OUT_PATH, "a");
    if (out) {
        fputs(line, out);
        fclose(out);
    }
}

static volatile u64 g_sink;

// One unit of integer ALU + branch work (1M xorshift steps).
static void unit_integer(void) {
    u64 x = 0x9E3779B97F4A7C15ull ^ g_sink, sink = 0;
    for (u32 i = 0; i < 1000000; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        if (x & 1)
            sink += x;
    }
    g_sink += sink;
}

// One unit of scalar floating point (500k iterations, double and float).
static void unit_float(void) {
    double acc = (double)(g_sink & 0xFF);
    float f = 1.0001f;
    for (u32 i = 0; i < 500000; i++) {
        acc += (double)i * 0.5 / (acc + 1.0);
        f = f * 1.0000001f + 0.5f;
    }
    g_sink += (u64)acc + (u64)f;
}

// One unit of guest memory traffic: copy 8 MiB and touch every page (the fastmem path).
// "memory" uses static buffers (the program image's BSS), "heap" malloc'd ones - where games
// keep most of their data. On the PS4 they can differ: fastmem there only covers memory whose
// guest and physical addresses line up at 16 KiB, which the heap does and the image may not.
#define MEM_SIZE (8u * 1024u * 1024u)
static u8 g_mem_a[MEM_SIZE], g_mem_b[MEM_SIZE];
static u8 *g_heap_a, *g_heap_b;
static void copy_unit(u8* dst, const u8* src) {
    memcpy(dst, src, MEM_SIZE);
    u64 sum = 0;
    for (size_t i = 0; i < MEM_SIZE; i += 4096)
        sum += dst[i];
    g_sink += sum;
}
static void unit_memory(void) {
    copy_unit(g_mem_b, g_mem_a);
}
static void unit_heap(void) {
    copy_unit(g_heap_b, g_heap_a);
}

// Fixed work per test. In single-core mode Eden's clock (system tick and time()) follows emulated
// cycles, so the guest can't time itself: the host timestamps the "begin"/"end" lines instead
// (Eden's PS4 status screen into boot-trace.log, scripts/bench-pc.sh on a PC).
static void run(const char* name, void (*unit)(void), u32 units) {
    report("begin %s\n", name);
    const u64 tick_start = armGetSystemTick();
    for (u32 i = 0; i < units; i++)
        unit();
    const u64 ticks = armGetSystemTick() - tick_start;
    report("end %s: %u units, %.2f s emulated\n", name, units, (double)ticks / TICKS_PER_SEC);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    FILE* truncate = fopen(OUT_PATH, "w");
    if (truncate)
        fclose(truncate);
    report("eden-ps4-test 4 (fixed work; real time measured by the host)\n");
    memset(g_mem_a, 0x5A, sizeof(g_mem_a));

    run("integer", unit_integer, 500);
    run("float", unit_float, 1000);
    run("memory", unit_memory, 100);
    g_heap_a = malloc(MEM_SIZE);
    g_heap_b = malloc(MEM_SIZE);
    if (g_heap_a && g_heap_b) {
        memset(g_heap_a, 0x5A, MEM_SIZE);
        run("heap", unit_heap, 100);
    } else {
        report("heap: malloc of 2 x 8 MiB failed\n");
    }

    report("done (checksum %llu)\n", (unsigned long long)(g_sink & 0xFFFF));
    return 0;
}
