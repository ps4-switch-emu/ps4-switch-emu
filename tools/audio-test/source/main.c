// PS4 Switch Emu: audio test. Plays tones through audout (48 kHz stereo s16) with on-screen text
// saying what should be heard, logging each step to sdmc:/eden-ps4-test.txt. ~25 s, then exits.
#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

#define RATE 48000
#define CHANNELS 2
#define FRAMES 2400 // 50 ms per buffer
#define BUFFER_BYTES ((FRAMES * CHANNELS * sizeof(s16) + 0xfff) & ~0xfff)
#define NUM_BUFFERS 4

typedef struct {
    const char* text;
    double freq;   // Hz; 0 = silence
    double left;   // volume 0..1
    double right;
    double seconds;
    bool scale;    // step through a C major scale instead of one frequency
} Step;

static const Step kSteps[] = {
    {"Silence", 0, 0, 0, 2, false},
    {"440 Hz tone, BOTH speakers", 440, 0.3, 0.3, 4, false},
    {"440 Hz tone, LEFT speaker only", 440, 0.3, 0, 4, false},
    {"660 Hz tone, RIGHT speaker only", 660, 0, 0.3, 4, false},
    {"Rising scale (8 notes)", 0, 0.3, 0.3, 4, true},
    {"Low 110 Hz hum, both", 110, 0.4, 0.4, 3, false},
    {"Silence", 0, 0, 0, 2, false},
};

static void Log(const char* line) {
    FILE* f = fopen("sdmc:/eden-ps4-test.txt", "a");
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

int main(int argc, char* argv[]) {
    consoleInit(NULL);
    FILE* f = fopen("sdmc:/eden-ps4-test.txt", "w");
    if (f) {
        fputs("audio-test\n", f);
        fclose(f);
    }

    Result rc = audoutInitialize();
    char line[160];
    snprintf(line, sizeof(line), "audoutInitialize: 0x%x, rate %u, channels %u", rc, audoutGetSampleRate(),
             audoutGetChannelCount());
    Log(line);
    if (R_SUCCEEDED(rc)) {
        rc = audoutStartAudioOut();
        snprintf(line, sizeof(line), "audoutStartAudioOut: 0x%x", rc);
        Log(line);
    }

    AudioOutBuffer buffers[NUM_BUFFERS];
    s16* data[NUM_BUFFERS];
    for (int i = 0; i < NUM_BUFFERS; i++) {
        data[i] = memalign(0x1000, BUFFER_BYTES);
        memset(data[i], 0, BUFFER_BYTES);
        buffers[i].next = NULL;
        buffers[i].buffer = data[i];
        buffers[i].buffer_size = BUFFER_BYTES;
        buffers[i].data_size = FRAMES * CHANNELS * sizeof(s16);
        buffers[i].data_offset = 0;
    }

    static const double kScale[] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25};
    double phase = 0;
    u64 frame_total = 0;
    int queued = 0, next = 0;
    for (size_t s = 0; s < sizeof(kSteps) / sizeof(kSteps[0]) && appletMainLoop(); s++) {
        const Step* st = &kSteps[s];
        snprintf(line, sizeof(line), "step %zu: %s", s + 1, st->text);
        Log(line);
        const u64 step_frames = (u64)(st->seconds * RATE);
        for (u64 done = 0; done < step_frames && appletMainLoop();) {
            printf("\x1b[1;1H");
            printf("PS4 Switch Emu - audio test                                 \n\n");
            printf(" audout: %s (0x%x)                                          \n\n",
                   R_SUCCEEDED(rc) ? "running" : "FAILED", rc);
            printf(" Step %zu of %zu:                                            \n", s + 1,
                   sizeof(kSteps) / sizeof(kSteps[0]));
            printf("   %-60s\n\n", st->text);
            printf(" %4.1f s of this step left                                  \n",
                   (double)(step_frames - done) / RATE);
            consoleUpdate(NULL);
            if (R_FAILED(rc)) {
                svcSleepThread(50000000);
                done += FRAMES;
                continue;
            }
            // Fill and queue a buffer, keeping NUM_BUFFERS in flight.
            if (queued == NUM_BUFFERS) {
                AudioOutBuffer* released = NULL;
                u32 count = 0;
                audoutWaitPlayFinish(&released, &count, UINT64_MAX);
                queued -= (int)count;
                if (queued < 0)
                    queued = 0;
            }
            s16* out = data[next];
            for (int i = 0; i < FRAMES; i++) {
                double freq = st->freq;
                if (st->scale) {
                    const size_t note = (size_t)((double)(done + (u64)i) / step_frames * 8);
                    freq = kScale[note < 8 ? note : 7];
                }
                const double v = freq > 0 ? sin(phase) : 0;
                phase += 2 * M_PI * freq / RATE;
                if (phase > 2 * M_PI)
                    phase -= 2 * M_PI;
                out[i * 2 + 0] = (s16)(v * st->left * 32767);
                out[i * 2 + 1] = (s16)(v * st->right * 32767);
            }
            audoutAppendAudioOutBuffer(&buffers[next]);
            queued++;
            next = (next + 1) % NUM_BUFFERS;
            done += FRAMES;
            frame_total += FRAMES;
        }
    }
    snprintf(line, sizeof(line), "done, %llu frames queued", (unsigned long long)frame_total);
    Log(line);
    if (R_SUCCEEDED(rc)) {
        audoutStopAudioOut();
        audoutExit();
    }
    consoleExit(NULL);
    return 0;
}
