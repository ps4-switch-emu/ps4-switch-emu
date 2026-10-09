// Renders the app's sounds (eden/externals/ps4sup/ps4_sounds.cpp) to WAV files on this computer, to
// listen to them without the console:
//   clang++ -std=c++20 -O2 -I../../eden/externals/ps4sup sounds.cpp -o sounds && ./sounds <out dir>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
extern "C" {
int32_t sceAudioOutInit() { return 0; }
int32_t sceAudioOutOpen(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t) { return -1; }
int32_t sceAudioOutOutput(int32_t, const void*) { return 0; }
int32_t sceAudioOutClose(int32_t) { return 0; }
}
#include "ps4_sounds.cpp"

static void WriteWav(const std::string& path, const std::vector<int16_t>& samples) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return;
    const uint32_t data = uint32_t(samples.size() * 2), rate = 48000;
    const auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    const auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f), u32(36 + data), fwrite("WAVEfmt ", 1, 8, f), u32(16), u16(1), u16(1), u32(rate),
        u32(rate * 2), u16(2), u16(16), fwrite("data", 1, 4, f), u32(data);
    fwrite(samples.data(), 2, samples.size(), f);
    fclose(f);
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    const std::pair<int, const char*> sounds[] = {
        {kSoundMove, "move"}, {kSoundConfirm, "confirm"}, {kSoundBack, "back"}, {kSoundToggle, "toggle"},
        {kSoundStartup, "startup"}, {kSoundLaunch, "launch"}, {kSoundMenuOpen, "menu-open"},
        {kSoundMenuClose, "menu-close"}, {kSoundError, "error"},
    };
    for (const auto& [sound, name] : sounds) {
        std::vector<int16_t> samples;
        float peak = 0;
        for (uint32_t t = 0;; t++) {
            bool done = false;
            const float v = std::tanh(Sample(sound, float(t) / kRate, done) * 1.5f) / 1.5f; // as the mixer does
            if (done)
                break;
            peak = std::max(peak, std::fabs(v));
            samples.push_back(int16_t(v * 32767.0f));
        }
        WriteWav(out + "/sound-" + name + ".wav", samples);
        printf("%-11s %5.2f s, peak %.2f\n", name, samples.size() / 48000.0, peak);
    }
}
