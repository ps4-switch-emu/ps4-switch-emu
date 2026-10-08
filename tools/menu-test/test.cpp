// Runs the PS4 launcher's menu logic (externals/ps4sup/ps4_menu.cpp) on a desktop computer: PS4 calls are
// stubbed, /data/... paths are redirected into a temporary folder, and the controller replays a
// script of button presses. Checks the files the menu leaves for Eden.
//   tools/menu-test/run.sh
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <vector>
#include <atomic>
#include <cmath>
#include <thread>

static std::string g_root; // stands in for "/"
static std::string Map(const char* p) {
    return (p && strncmp(p, "/data/", 6) == 0) ? g_root + p : std::string(p ? p : "");
}
static FILE* t_fopen(const char* p, const char* m) { return fopen(Map(p).c_str(), m); }
static DIR* t_opendir(const char* p) { return opendir(Map(p).c_str()); }
static int t_stat(const char* p, struct stat* st) { return stat(Map(p).c_str(), st); }
static int t_mkdir(const char* p, mode_t m) { return mkdir(Map(p).c_str(), m); }
static int t_remove(const char* p) { return remove(Map(p).c_str()); }
static int t_rename(const char* a, const char* b) { return rename(Map(a).c_str(), Map(b).c_str()); }
#define fopen t_fopen
#define opendir t_opendir
#define stat(p, s) t_stat(p, s)
#define mkdir t_mkdir
#define remove t_remove
#define rename t_rename
#define usleep(x) ((void)0)
#define sleep(x) ((void)0)

// Controller script: one entry per poll.
static std::vector<uint32_t> g_script;
static size_t g_step = 0;
static bool g_exec = false;
extern "C" {
void ps4_status_set_menu(const char* const*, int) {}
void ps4_status_set_stage(const char*) {}
void ps4_status_watch_file(const char*, const char*) {}
int ps4_status_copy_watch(char* out, int size) { if (size) out[0] = 0; return 0; }
void ps4_boot_trace(const char* s) { printf("    trace: %s\n", s); }
int32_t sceUserServiceGetInitialUser(int32_t* user) { *user = 1; return 0; }
int32_t scePadInit() { return 0; }
int32_t scePadOpen(int32_t, int32_t, int32_t, const void*) { return 7; }
int32_t scePadClose(int32_t) { return 0; }
int32_t scePadReadState(int32_t, void* data) {
    if (g_step >= g_script.size()) {
        printf("FAIL: controller script ran out (menu still waiting)\n");
        exit(1);
    }
    *static_cast<uint32_t*>(data) = g_script[g_step++];
    return 0;
}
int sceSystemServiceLoadExec(const char*, char* const*) { g_exec = true; return 0; }
int32_t sceAudioOutInit() { return 0; }
int32_t sceAudioOutOpen(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t) { return 5; }
int32_t sceAudioOutOutput(int32_t, const void*) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); return 0; }
int32_t sceAudioOutClose(int32_t) { return 0; }
unsigned char* stbi_load(const char*, int*, int*, int*, int) { return nullptr; }
unsigned char* stbi_load_from_memory(const unsigned char*, int, int*, int*, int*, int) { return nullptr; }
void stbi_image_free(void*) {}
// Launcher drawing: record what's shown.
#include "../../eden/externals/ps4sup/ps4_launcher.h"
static std::vector<std::string> g_setting_labels, g_setting_values;
static std::string g_scope;
static int g_tab, g_game, g_row;
void ps4_launcher_set_games(const Ps4LauncherGame*, int) {}
void ps4_launcher_set_settings(const Ps4LauncherSetting* s, int n) {
    g_setting_labels.clear();
    g_setting_values.clear();
    for (int i = 0; i < n; i++) {
        g_setting_labels.push_back(s[i].label);
        g_setting_values.push_back(s[i].value);
    }
}
void ps4_launcher_set_state(int tab, int game, int row) { g_tab = tab; g_game = game; g_row = row; }
void ps4_launcher_set_info(int, bool) {}
void ps4_launcher_show(bool) {}
void ps4_launcher_set_loading(const char*, const uint32_t*) {}
void ps4_launcher_set_scope(const char* t, bool) { g_scope = t; }
void ps4_launcher_toast(const char* t) { printf("    toast: %s\n", t); }
}

#include "../../eden/externals/ps4sup/ps4_menu.cpp"
#undef stat

enum : uint32_t { Up = 0x10, Right = 0x20, Down = 0x40, Left = 0x80, L1 = 0x400, R1 = 0x800, Tri = 0x1000,
                  Cir = 0x2000, X = 0x4000, Sq = 0x8000 };

// Each press is followed by a release poll.
static void Script(std::initializer_list<uint32_t> presses) {
    g_script.clear();
    g_step = 0;
    g_script.push_back(0); // first poll: nothing held
    for (uint32_t b : presses) {
        g_script.push_back(b);
        g_script.push_back(0);
    }
}

static bool Exists(const std::string& p) {
    struct stat st;
    return ::stat((g_root + p).c_str(), &st) == 0;
}
static std::string Read(const std::string& p) {
    std::string out;
    if (FILE* f = ::fopen((g_root + p).c_str(), "r")) {
        char buf[512];
        while (fgets(buf, sizeof(buf), f))
            out += buf;
        ::fclose(f);
    }
    return out;
}
static int g_failures = 0;
static void Check(bool ok, const char* what) {
    printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    g_failures += !ok;
}
static void Played(const std::string& file, long when) {
    ::mkdir((g_root + "/data/switch-emu/titles").c_str(), 0777);
    if (FILE* f = ::fopen((g_root + "/data/switch-emu/titles/" + file + ".last").c_str(), "w")) {
        fprintf(f, "%ld\n", when);
        ::fclose(f);
    }
}
static void Chosen(const std::string& file) {
    if (FILE* f = ::fopen((g_root + "/data/switch-emu/game.txt").c_str(), "w")) {
        fprintf(f, "%s\n", file.c_str());
        ::fclose(f);
    }
}
static void Touch(const std::string& p) {
    if (FILE* f = ::fopen((g_root + p).c_str(), "w"))
        ::fclose(f);
}

int main() {
    char tmpl[] = "/tmp/eden-menu-test-XXXXXX";
    g_root = mkdtemp(tmpl);
    for (const char* d : {"/data", "/data/switch-emu", "/data/switch-emu/games", "/data/switch-emu/install"})
        ::mkdir((g_root + d).c_str(), 0777);
    Touch("/data/switch-emu/games/Sample Platformer [0100000000010000][v0][Base].nsp");
    Touch("/data/switch-emu/games/Puzzle Party.nsp");
    Touch("/data/switch-emu/games/gpu-test.nro");

    printf("1. Start the first game (names sorted: Puzzle Party, Sample Platformer, gpu-test)\n");
    Script({X});
    ps4_menu_run();
    Check(ps4_menu_game_path() == std::string("/data/switch-emu/games/Puzzle Party.nsp"), "starts Puzzle Party");
    Check(Exists("/data/switch-emu/titles/Puzzle Party.nsp.last"), "marks it played");
    Check(!Exists("/data/switch-emu/handheld"), "docked by default");

    printf("2. Sample Platformer: own settings Handheld + 0.75x, then start it\n");
    // Recently played first: Puzzle Party is now first; Right -> Sample Platformer.
    Script({Right, Sq, Down, X, Down, X, Cir, X});
    ps4_menu_run();
    Check(ps4_menu_game_path() == std::string("/data/switch-emu/games/Sample Platformer [0100000000010000][v0][Base].nsp"),
          "starts Sample Platformer");
    const std::string opts = Read("/data/switch-emu/titles/Sample Platformer [0100000000010000][v0][Base].nsp.opts");
    Check(opts.find("handheld=1") != std::string::npos, "its .opts has handheld=1");
    Check(opts.find("resolution=1") != std::string::npos, "its .opts has resolution=1 (0.75x)");
    Check(Exists("/data/switch-emu/handheld") && Exists("/data/switch-emu/res-075"), "this run: handheld, 0.75x");
    Check(Read("/data/switch-emu/options.txt").find("handheld=1") == std::string::npos, "all-games settings untouched");

    printf("3. Puzzle Party again: follows the all-games settings\n");
    const std::string smb = "Sample Platformer [0100000000010000][v0][Base].nsp";
    Played(smb, 2000);
    Played("Puzzle Party.nsp", 1000);
    Chosen(smb); // order: Sample Platformer, Puzzle Party, gpu-test; selection on Sample Platformer
    Script({Right, X});
    ps4_menu_run();
    Check(ps4_menu_game_path() == std::string("/data/switch-emu/games/Puzzle Party.nsp"), "starts Puzzle Party");
    Check(!Exists("/data/switch-emu/handheld") && !Exists("/data/switch-emu/res-075"), "this run: docked, 1x");

    printf("4. All-games settings: GPU accuracy High (R1, Down x3, Cross), start the first game\n");
    Played("Puzzle Party.nsp", 3000);
    Chosen("Puzzle Party.nsp"); // order: Puzzle Party, Sample Platformer, gpu-test
    Script({R1, Down, Down, Down, X, L1, X});
    ps4_menu_run();
    Check(Read("/data/switch-emu/options.txt").find("gpu_high=1") != std::string::npos, "options.txt has gpu_high=1");
    Check(Exists("/data/switch-emu/gpu-high"), "this run: GPU accuracy High");

    printf("5. Sample Platformer: Use the settings for all games\n");
    Played("Puzzle Party.nsp", 4000);
    Chosen("Puzzle Party.nsp"); // order: Puzzle Party, Sample Platformer, gpu-test
    // Square on Sample Platformer, Up (wraps to the last row), Cross, back, start (Sample Platformer).
    Script({Right, Sq, Up, X, Cir, X});
    ps4_menu_run();
    Check(!Exists("/data/switch-emu/titles/Sample Platformer [0100000000010000][v0][Base].nsp.opts"), "its .opts removed");
    Check(ps4_menu_game_path() == "/data/switch-emu/games/" + smb, "starts Sample Platformer");
    Check(!Exists("/data/switch-emu/handheld") && Exists("/data/switch-emu/gpu-high"), "this run: all-games settings");

    printf("6. Back up saves with a save present\n");
    for (const char* d : {"/data/switch-emu/nand", "/data/switch-emu/nand/user", "/data/switch-emu/nand/user/save",
                          "/data/switch-emu/nand/user/save/0000000000000000"})
        ::mkdir((g_root + d).c_str(), 0777);
    Touch("/data/switch-emu/nand/user/save/0000000000000000/save.bin");
    // R1, Up x3 (wraps to the last row, Firmware, then up past Keys to Back up saves), Cross, L1,
    // Cross (start).
    Script({R1, Up, Up, Up, X, L1, X});
    ps4_menu_run();
    bool backed_up = false;
    if (DIR* d = ::opendir((g_root + "/data/switch-emu/save-backups").c_str())) {
        while (dirent* e = readdir(d))
            if (e->d_name[0] != '.')
                backed_up |= Exists(std::string("/data/switch-emu/save-backups/") + e->d_name + "/0000000000000000/save.bin");
        closedir(d);
    }
    Check(backed_up, "save.bin copied into save-backups/<date>/");

    printf("7. Install updates: Triangle with a file waiting\n");
    Touch("/data/switch-emu/install/update.nsp");
    Script({Tri});
    ps4_menu_run();
    Check(Exists("/data/switch-emu/install-now"), "install-now requested");

    printf("8. Home menu Restart game: straight back into the same game\n");
    remove("/data/switch-emu/install-now");
    Chosen(smb);
    Touch("/data/switch-emu/autostart");
    Script({}); // the controller must not be read
    g_script.clear();
    ps4_menu_run();
    Check(ps4_menu_game_path() == "/data/switch-emu/games/" + smb, "restarts Sample Platformer");
    Check(!Exists("/data/switch-emu/autostart"), "autostart consumed");

    printf("9. Homebrew titles and icons straight from the .nro files in out/\n");
    for (const char* nro : {"gfx-test.nro", "gpu-test.nro", "pad-test.nro", "audio-test.nro", "test.nro"}) {
        const std::string src = std::string(OUT_DIR) + "/" + nro;
        const std::string dst = g_root + "/data/switch-emu/games/" + nro;
        FILE* in = ::fopen(src.c_str(), "rb");
        if (!in)
            continue;
        FILE* out = ::fopen(dst.c_str(), "wb");
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, n, out);
        ::fclose(in);
        ::fclose(out);
        NroAssets assets;
        const bool ok = ReadNroAssets(nro, assets);
        printf("    %-16s title \"%s\", icon %zu bytes\n", nro, assets.title.c_str(), assets.icon_jpeg.size());
        Check(ok && !assets.title.empty(), "title read (icon optional: our test programs have none)");
    }

    printf("\n%s (%s)\n", g_failures ? "FAILED" : "all passed", g_root.c_str());
    return g_failures ? 1 : 0;
}
