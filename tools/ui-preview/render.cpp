// Renders the PS4 launcher on a desktop computer to check its layout: includes status_screen.cpp with the
// PS4 system calls stubbed out.
#include <cstdint>
#include <cstdio>
#include <sys/types.h>
extern "C" {
int32_t sceVideoOutOpen(int32_t, int32_t, int32_t, const void*) { return -1; }
int32_t sceVideoOutClose(int32_t) { return 0; }
int32_t sceVideoOutRegisterBuffers(int32_t, int32_t, void* const*, int32_t, const void*) { return 0; }
int32_t sceVideoOutUnregisterBuffers(int32_t, int32_t) { return 0; }
int32_t sceVideoOutSubmitFlip(int32_t, int32_t, uint32_t, int64_t) { return 0; }
int32_t sceVideoOutSetFlipRate(int32_t, int32_t) { return 0; }
int32_t sceVideoOutIsFlipPending(int32_t) { return 0; }
int32_t sceKernelAllocateDirectMemory(off_t, off_t, size_t, size_t, int32_t, off_t*) { return -1; }
int32_t sceKernelMapDirectMemory(void**, size_t, int32_t, int32_t, off_t, size_t) { return -1; }
int32_t sceKernelMunmap(void*, size_t) { return 0; }
int32_t sceKernelReleaseDirectMemory(off_t, size_t) { return 0; }
size_t sceKernelGetDirectMemorySize() { return 0; }
void ps4_boot_trace(const char*) {}
}
#include "status_screen.cpp"
#include <vector>
#include <thread>
#include <string>
#include <iterator>
#include <chrono>

static void Save(const char* path) {
    FILE* f = fopen(path, "wb");
    fprintf(f, "P6 %d %d 255\n", kWidth, kHeight);
    for (int i = 0; i < kWidth * kHeight; i++) {
        const uint32_t p = g_frame[i];
        const unsigned char rgb[3] = {uint8_t(p), uint8_t(p >> 8), uint8_t(p >> 16)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static std::string g_out;
static void Shot(const char* name) {
    const std::string path = g_out + "/" + name + ".ppm";
    Save(path.c_str());
}
static void Settle() {
    for (int i = 0; i < 60; i++) { // let animations finish, in real time as on the console
        DrawFrame(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

// A stand-in icon: diagonal gradient between two colours with a soft circle.
static std::vector<uint32_t> MakeIcon(uint32_t a, uint32_t b) {
    std::vector<uint32_t> icon(256 * 256);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            const float t = (x + y) / 510.0f;
            uint32_t c = Mix(a, b, t);
            const int dx = x - 170, dy = y - 90;
            if (dx * dx + dy * dy < 60 * 60)
                c = Mix(c, Rgb(255, 255, 255), 0.35f);
            icon[size_t(y) * 256 + x] = c;
        }
    return icon;
}

static Ps4LauncherSetting S(const char* l, const char* v, const char* h, const char* g, int k) {
    Ps4LauncherSetting s{l, v, h};
    s.group = g;
    s.kind = k;
    return s;
}

int main(int argc, char** argv) {
    g_out = argc > 1 ? argv[1] : ".";
    g_frame = static_cast<uint32_t*>(malloc(kFrameBytes));
    clock_gettime(CLOCK_MONOTONIC, &g_start); // the status screen's "running" clock
    // A sample library: placeholder games plus the test programs. Icons appear after a game's first
    // start; the first game (played) and the homebrew have theirs.
    const auto smb = MakeIcon(Rgb(200, 30, 40), Rgb(90, 10, 20));
    const auto hb = MakeIcon(Rgb(40, 120, 220), Rgb(20, 40, 100));
    static const char* const names[] = {"Sample Platformer", "Sample Puzzle Game", "Sample Adventure",
        "Sample Racing Game", "Sample Farming Game", "Sample RPG Chapter 1&2", "Sample Indie Game", "Sample Party Game",
        "Sample Kart Racer", "Sample 3D Platformer", "Sample Open-World Adventure: A Very Long Title", "Sample Shooter",
        "Sample Action Game", "Sample Sandbox", "Sample Co-op Game", "Sample Arcade Game", "Sample Puzzle Duo",
        "Sample Fighter", "Sample Mini-Games", "Sample Ghost Game", "Sample Monster Game", "Sample JRPG",
        "Sample Life Sim", "Sample Brawler", "Sample Side-Scroller", "Sample Climbing Game", "Graphics Test",
        "Controller Test", "Audio Test", "gpu-test"};
    std::vector<std::string> details;
    std::vector<Ps4LauncherGame> library;
    for (const char* n : names) {
        const bool homebrew = strstr(n, " Test") != nullptr || strcmp(n, "gpu-test") == 0;
        details.push_back(strcmp(n, "Sample Platformer") == 0 ? "Game  -  NSP, 203 MB  -  last played Oct 08"
                          : homebrew                       ? "Homebrew  -  NRO, 0 MB"
                                                           : "Game  -  NSP, 3.4 GB");
    }
    for (size_t i = 0; i < std::size(names); i++) {
        const bool homebrew = strstr(names[i], " Test") != nullptr || strcmp(names[i], "gpu-test") == 0;
        library.push_back({names[i], details[i].c_str(), i == 0 ? smb.data() : homebrew ? hb.data() : nullptr});
    }
    ps4_launcher_set_games(library.data(), int(library.size()));
    ps4_launcher_set_info(1, true);

    const char* kGraphicsHelp = "Vulkan draws the game on the PS4's GPU. Off runs without graphics and shows this "
                                "status screen instead (CPU tests).";
    const char* kSpreadHelp = "Lets the game's helper threads (music, loading) run on idle CPU cores instead of "
                              "sharing the main one - fewer lag spikes. Turn off if a game misbehaves.";
    const char* kBackupHelp = "Copies all save data to /data/switch-emu/save-backups/<date and time>. To restore, copy a "
                              "backup's folders back into /data/switch-emu/nand/user/save.";
    std::vector<Ps4LauncherSetting> all = {
        S("Graphics", "Vulkan", kGraphicsHelp, "Display", kSettingChoice),
        S("Console mode", "Docked", "Handheld makes games render at 720p instead of 1080p: much less GPU work, often a big speed-up on the PS4. Docked looks sharper.", "Display", kSettingChoice),
        S("Resolution", "1x (native)", "x", "Display", kSettingChoice),
        S("GPU accuracy", "Low", "x", "Display", kSettingChoice),
        S("Button layout", "Switch positions", "x", "Controls", kSettingChoice),
        S("Spread game threads", "Off", kSpreadHelp, "Performance", kSettingToggle),
        S("Multi-core", "On", "x", "Performance", kSettingToggle),
        S("Fastmem", "On", "x", "Performance", kSettingToggle),
        S("CPU accuracy", "Auto", "x", "Performance", kSettingChoice),
        S("Async shaders", "Off", "x", "Performance", kSettingToggle),
        S("Performance overlay", "Detailed", "Shows the frame rate in a corner while playing. Detailed adds frame times, how much time is spent waiting for the GPU, new shaders and free GPU memory. Also in the Home menu (hold L3 + R3).", "Debug", kSettingChoice),
        S("Overlay position", "Top right", "Which corner of the screen the performance overlay sits in.", "Debug", kSettingChoice),
        S("Profiler", "Off", "x", "Debug", kSettingToggle),
        S("Run all benchmarks", "Start", "x", "Tools", kSettingAction),
        S("Install updates and DLC", "1 file", "x", "Tools", kSettingAction),
        S("Back up saves", "Done: 12 files", kBackupHelp, "Tools", kSettingAction),
        S("Keys", "Found", "prod.keys and title.keys, dumped from your own Switch, go in /data/switch-emu/keys. Dump them after updating the Switch to the firmware you use here. Cross checks again.", "Setup", kSettingText),
        S("Firmware", "Installed (234 files)", "The Switch firmware dumped from your own console: copy its .nca files, loose, into the folder nand/system/Contents/registered inside /data/switch-emu. Cross checks again.", "Setup", kSettingText),
        S("Version", "v00.66", "PS4 Switch Emu, an unofficial port of the Eden emulator. Please include this version and the PS4 firmware below in bug reports.", "About", kSettingText),
        S("PS4 firmware", "13.02", "The console's system software version, as the console reports it.", "About", kSettingText),
    };
    std::vector<Ps4LauncherSetting> game = {
        S("Graphics", "Vulkan", kGraphicsHelp, "Display", kSettingChoice),
        S("Console mode", "Handheld", "Handheld makes games render at 720p instead of 1080p: much less GPU work, often a big speed-up on the PS4. Docked looks sharper.", "Display", kSettingChoice),
        S("Resolution", "0.75x", "x", "Display", kSettingChoice),
        S("GPU accuracy", "Low", "x", "Display", kSettingChoice),
        S("Button layout", "Cross = A", "x", "Controls", kSettingChoice),
        S("Spread game threads", "On", kSpreadHelp, "Performance", kSettingToggle),
        S("Multi-core", "On", "x", "Performance", kSettingToggle),
        S("Fastmem", "On", "x", "Performance", kSettingToggle),
        S("CPU accuracy", "Unsafe", "x", "Performance", kSettingChoice),
        S("Async shaders", "On", "x", "Performance", kSettingToggle),
        S("Profiler", "Off", "x", "Tools", kSettingToggle),
        S("Use the settings for all games", "Reset", "Removes this game's own settings; it then follows the settings for all games again.", "This game", kSettingAction),
    };
    ps4_launcher_show(true);

    ps4_launcher_set_state(0, 0, 0);
    Settle();
    Shot("01-games");
    ps4_launcher_set_state(0, 21, 0);
    Settle();
    ps4_launcher_toast("Sample Platformer now has its own settings");
    DrawFrame(0);
    Shot("02-games-scrolled");
    g_toast[0] = 0;
    ps4_launcher_set_layout(kLayoutRow, -1);
    ps4_launcher_set_state(0, 2, 0);
    Settle();
    Shot("02b-games-row");
    ps4_launcher_set_layout(kLayoutRow, kShortcutBackup);
    Settle();
    Shot("02c-games-row-shortcuts");
    ps4_launcher_set_layout(kLayoutRow, -1);
    for (int i = 3; i <= 9; i++) { // walk right: the row scrolls once the selection reaches the edge
        ps4_launcher_set_state(0, i, 0);
        Settle();
    }
    Shot("02d-games-row-scrolled");
    ps4_launcher_set_layout(kLayoutGrid, -1);
    {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 100; i++)
            DrawFrame(0);
        fprintf(stderr, "games grid: %.2f ms per frame on this machine\n",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 100);
    }
    g_toast[0] = 0;
    ps4_launcher_set_settings(all.data(), int(all.size()));
    ps4_launcher_set_scope("All games", true);
    ps4_launcher_set_state(1, 0, 0);
    Settle();
    Shot("03-settings");
    ps4_launcher_set_state(1, 0, 5);
    Settle();
    Shot("04-settings-performance");
    ps4_launcher_set_state(1, 0, 10);
    Settle();
    Shot("05a-settings-debug");
    ps4_launcher_set_state(1, 0, 15);
    Settle();
    ps4_launcher_toast("Save backup: Done: 12 files");
    DrawFrame(0);
    Shot("05-settings-tools");
    g_toast[0] = 0;
    ps4_launcher_set_state(1, 0, int(all.size()) - 1);
    Settle();
    Shot("05b-settings-setup");
    ps4_launcher_set_settings(game.data(), int(game.size()));
    ps4_launcher_set_scope("Sample Platformer  (own settings)", true);
    ps4_launcher_set_state(1, 0, 1);
    Settle();
    Shot("06-settings-this-game");
    ps4_launcher_set_games(library.data(), 0);
    ps4_launcher_set_info(0, false);
    ps4_launcher_toast("Keys missing - see Settings > Setup");
    ps4_launcher_set_state(0, 0, 0);
    Settle();
    Shot("07-empty-library");

    // The launch zoom, partway: from the grid's selected card toward the loading screen's.
    ps4_launcher_show(true);
    ps4_launcher_set_games(library.data(), int(library.size()));
    ps4_launcher_set_info(1, true);
    ps4_launcher_set_layout(kLayoutGrid, -1);
    ps4_launcher_set_state(0, 0, 0);
    Settle();
    g_launch_snapped = false;
    g_launch_t0 = Elapsed();
    for (int i = 0; i < 7; i++) {
        DrawFrame(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    Shot("08a-launch-zoom");
    g_launch_t0 = -1;
    ps4_launcher_set_loading("Sample Platformer", smb.data());
    std::vector<uint32_t> fb(size_t(kWidth) * kHeight);
    ps4_status_render_loading(fb.data(), kWidth, "Building shaders from the cache: 120 of 431");
    memcpy(g_frame, fb.data(), kFrameBytes);
    Shot("08-loading");

    // Home menu over a stand-in game frame.
    for (int y = 0; y < kHeight; y++)
        for (int x = 0; x < kWidth; x++) {
            uint32_t c = Mix(Rgb(110, 170, 230), Rgb(250, 220, 180), float(y) / kHeight);
            if (y > 760)
                c = ((x / 80 + y / 80) & 1) ? Rgb(90, 60, 40) : Rgb(70, 45, 30);
            const int dx = x - 960, dy = y - 700;
            if (dx * dx + dy * dy < 50 * 50)
                c = Rgb(200, 30, 40);
            fb[size_t(y) * kWidth + x] = c;
        }
    static const char* const items[] = {"Resume", "Restart game", "Quit to launcher", "Performance overlay: Detailed"};
    Ps4HomeMenu menu{"Sample Platformer", "Frame rate: 58 fps (last 5 s)", items, 4, 0};
    std::vector<uint32_t> out(size_t(kWidth) * kHeight);
    ps4_status_draw_home(out.data(), fb.data(), kWidth, &menu, true);
    for (int i = 0; i < 30 && ps4_status_home_animating(); i++) { // the panel rising in
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ps4_status_draw_home(out.data(), fb.data(), kWidth, &menu, false);
    }
    memcpy(g_frame, out.data(), kFrameBytes);
    Shot("09-home-menu");
    menu.selected = 2; // the highlight gliding down two items: a frame partway
    ps4_status_draw_home(out.data(), fb.data(), kWidth, &menu, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    ps4_status_draw_home(out.data(), fb.data(), kWidth, &menu, false);
    memcpy(g_frame, out.data(), kFrameBytes);
    Shot("09b-home-menu-moving");

    // Performance overlay stamped onto the stand-in game frame (as the GPU copy does).
    {
        const auto stamp = [&](int mode, int corner, const Ps4OverlayStats& st) {
            const int w = mode == kOverlayFps ? kOverlayFpsW : kOverlayDetailW;
            const int h = mode == kOverlayFps ? kOverlayFpsH : kOverlayDetailH;
            std::vector<uint32_t> panel(size_t(w) * h);
            Ps4OverlayStats copy = st;
            copy.mode = mode;
            ps4_status_render_overlay(panel.data(), w, h, &copy);
            const int x = (corner & 1) ? kWidth - w - 28 : 28, y = (corner & 2) ? kHeight - h - 28 : 28;
            for (int r = 0; r < h; r++)
                memcpy(g_frame + size_t(y + r) * kWidth + x, panel.data() + size_t(r) * w, size_t(w) * 4);
        };
        Ps4OverlayStats st{};
        st.fps = 58.4f, st.frame_ms = 17.1f, st.worst_ms = 33.4f, st.gpu_wait = 0.12f, st.new_shaders = 3,
        st.gpu_free_mb = 359;
        memcpy(g_frame, fb.data(), kFrameBytes);
        stamp(kOverlayFps, kOverlayTopRight, st);
        Shot("11-overlay-fps");
        st.fps = 34.2f, st.frame_ms = 29.2f, st.worst_ms = 183.0f, st.gpu_wait = 0.41f;
        memcpy(g_frame, fb.data(), kFrameBytes);
        stamp(kOverlayDetailed, kOverlayTopLeft, st);
        Shot("12-overlay-detailed");
    }

    // The status screen (graphics off: CPU tests and the benchmark suite).
    ps4_launcher_show(false);
    for (const char* line : {"main: eden-cli starting, heap 512 MiB from system flexible memory",
                             "host memory: 4096 MiB emulated DRAM; direct memory 4608 MiB total",
                             "fastmem: reserved 16 GiB arena at 0x201200000", "main: system initialized",
                             "main: loading game", "main: load result 0", "main: running emulation"})
        ps4_status_push(line);
    ps4_status_set_stage("Benchmark suite 2/5: multi-core, fastmem");
    DrawFrame(1);
    Shot("10-status-screen");
}
