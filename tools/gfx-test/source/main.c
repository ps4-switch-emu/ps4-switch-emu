// PS4 Switch Emu graphics bring-up: draws with the CPU into the Switch framebuffer (no GPU shaders),
// so it tests the display path - Eden's presentation through Vulkan to the PS4's VideoOut - on
// its own. Moving colour bars plus a white square that bounces, so a frozen picture is obvious.
// Press + to exit.

#include <string.h>
#include <switch.h>

#define WIDTH 1280
#define HEIGHT 720

static u32 rgba(u8 r, u8 g, u8 b) {
    return RGBA8_MAXALPHA(r, g, b);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    Framebuffer fb;
    framebufferCreate(&fb, nwindowGetDefault(), WIDTH, HEIGHT, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    static const u32 bars[8] = {0xFFFFFFFF, 0xFF00FFFF, 0xFFFFFF00, 0xFF00FF00,
                                0xFFFF00FF, 0xFF0000FF, 0xFFFF0000, 0xFF000000};
    u32 frame = 0;
    int square_x = 100, square_y = 100, dx = 7, dy = 5;
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;

        u32 stride;
        u32* pixels = (u32*)framebufferBegin(&fb, &stride);
        const u32 pitch = stride / sizeof(u32);
        for (u32 y = 0; y < HEIGHT; y++) {
            u32* row = pixels + y * pitch;
            for (u32 x = 0; x < WIDTH; x++)
                row[x] = bars[((x + frame * 4) / (WIDTH / 8)) % 8];
        }
        // A bar at the bottom whose colour cycles every frame.
        for (u32 y = HEIGHT - 60; y < HEIGHT; y++)
            for (u32 x = 0; x < WIDTH; x++)
                pixels[y * pitch + x] = rgba((u8)(frame * 3), (u8)(frame * 5), (u8)(frame * 7));
        // The bouncing square.
        square_x += dx;
        square_y += dy;
        if (square_x < 0 || square_x > WIDTH - 120) dx = -dx;
        if (square_y < 0 || square_y > HEIGHT - 180) dy = -dy;
        for (int y = square_y; y < square_y + 120; y++)
            for (int x = square_x; x < square_x + 120; x++)
                pixels[y * pitch + x] = rgba(255, 255, 255);
        framebufferEnd(&fb);
        frame++;
    }
    framebufferClose(&fb);
    return 0;
}
