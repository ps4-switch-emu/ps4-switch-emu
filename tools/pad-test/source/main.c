// PS4 Switch Emu: controller test. Shows what the emulated Switch controller reports - buttons,
// sticks, controller style - on a text console, and logs every change to sdmc:/eden-ps4-test.txt.
// Exits after 30 s without input, or when + and - are held together.
#include <stdio.h>
#include <string.h>
#include <switch.h>

static const struct {
    u64 mask;
    const char* name;
} kButtons[] = {
    {HidNpadButton_A, "A"},       {HidNpadButton_B, "B"},         {HidNpadButton_X, "X"},
    {HidNpadButton_Y, "Y"},       {HidNpadButton_L, "L"},         {HidNpadButton_R, "R"},
    {HidNpadButton_ZL, "ZL"},     {HidNpadButton_ZR, "ZR"},       {HidNpadButton_Plus, "+"},
    {HidNpadButton_Minus, "-"},   {HidNpadButton_StickL, "LStick"}, {HidNpadButton_StickR, "RStick"},
    {HidNpadButton_Up, "Up"},     {HidNpadButton_Down, "Down"},   {HidNpadButton_Left, "Left"},
    {HidNpadButton_Right, "Right"},
};

static void ButtonList(u64 held, char* out, size_t size) {
    out[0] = 0;
    for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) {
        if (held & kButtons[i].mask) {
            strncat(out, kButtons[i].name, size - strlen(out) - 1);
            strncat(out, " ", size - strlen(out) - 1);
        }
    }
    if (!out[0])
        snprintf(out, size, "(none)");
}

static void Log(const char* line) {
    FILE* f = fopen("sdmc:/eden-ps4-test.txt", "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);
    padConfigureInput(8, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeAny(&pad);

    FILE* f = fopen("sdmc:/eden-ps4-test.txt", "w");
    if (f) {
        fputs("pad-test: controller test\n", f);
        fclose(f);
    }

    const u64 start = armGetSystemTick();
    u64 last_input = start;
    u64 last_held = ~0ull;
    int last_lx = 99999, last_ly = 99999, last_rx = 99999, last_ry = 99999;
    u32 frames = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 held = padGetButtons(&pad);
        const HidAnalogStickState l = padGetStickPos(&pad, 0);
        const HidAnalogStickState r = padGetStickPos(&pad, 1);
        const u32 style = padGetStyleSet(&pad);
        const bool connected = padIsConnected(&pad);
        const u64 now = armGetSystemTick();

        char buttons[160];
        ButtonList(held, buttons, sizeof(buttons));
        const bool sticks_moved = (l.x / 4096 != last_lx / 4096) || (l.y / 4096 != last_ly / 4096) ||
                                  (r.x / 4096 != last_rx / 4096) || (r.y / 4096 != last_ry / 4096);
        if (held != last_held || sticks_moved) {
            if (last_held != ~0ull)
                last_input = now;
            char line[256];
            snprintf(line, sizeof(line), "%6.1f s: buttons %s | L %6d %6d | R %6d %6d", armTicksToNs(now - start) / 1e9,
                     buttons, l.x, l.y, r.x, r.y);
            Log(line);
            last_held = held;
            last_lx = l.x, last_ly = l.y, last_rx = r.x, last_ry = r.y;
        }

        const double idle = armTicksToNs(now - last_input) / 1e9;
        printf("\x1b[1;1H");
        printf("PS4 Switch Emu - controller test                                    \n\n");
        printf(" Controller connected: %-3s   style set: 0x%08x                     \n", connected ? "yes" : "NO",
               style);
        printf(" Frame %u                                                            \n\n", frames++);
        printf(" Buttons held: %-60s\n\n", buttons);
        printf(" Left stick:  x %6d   y %6d                                     \n", l.x, l.y);
        printf(" Right stick: x %6d   y %6d                                     \n\n", r.x, r.y);
        printf(" PS4 -> Switch: Cross=B  Circle=A  Square=Y  Triangle=X\n");
        printf("                L1=L  R1=R  L2=ZL  R2=ZR  Options=+  Touchpad=-\n\n");
        printf(" Hold + and - together to exit. Exits by itself after 30 s with no input\n");
        printf(" (%2.0f s left).                                                      \n", idle < 30 ? 30 - idle : 0);
        consoleUpdate(NULL);

        if ((held & HidNpadButton_Plus) && (held & HidNpadButton_Minus))
            break;
        if (idle > 30)
            break;
    }
    Log("exit");
    consoleExit(NULL);
    return 0;
}
