#include "div_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define _ESC 0x01
#define _A 0x1E
#define _W 0x11
#define _S 0x1F
#define _SPC 0x39

#define LOGICAL_W 320
#define LOGICAL_H 200
#define WINDOW_SCALE 3

static void hsv_to_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float i = floorf(h * 6.0f);
    float f = h * 6.0f - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    int idx = ((int)i) % 6;
    float rr, gg, bb;
    switch (idx) {
        case 0: rr = v; gg = t; bb = p; break;
        case 1: rr = q; gg = v; bb = p; break;
        case 2: rr = p; gg = v; bb = t; break;
        case 3: rr = p; gg = q; bb = v; break;
        case 4: rr = t; gg = p; bb = v; break;
        default: rr = v; gg = p; bb = q; break;
    }
    *r = (uint8_t)(rr * 255.0f);
    *g = (uint8_t)(gg * 255.0f);
    *b = (uint8_t)(bb * 255.0f);
}

static void synth_wav(uint8_t **out, long *outlen, int nsamples, double hz)
{
    int sampleRate = 22050;
    long dataLen = nsamples * 2;
    long total = 44 + dataLen;
    uint8_t *buf = (uint8_t *)malloc(total);

    memcpy(buf + 0, "RIFF", 4);
    *(uint32_t *)(buf + 4) = (uint32_t)(36 + dataLen);
    memcpy(buf + 8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    *(uint32_t *)(buf + 16) = 16;
    *(uint16_t *)(buf + 20) = 1;
    *(uint16_t *)(buf + 22) = 1;
    *(uint32_t *)(buf + 24) = sampleRate;
    *(uint32_t *)(buf + 28) = sampleRate * 2;
    *(uint16_t *)(buf + 32) = 2;
    *(uint16_t *)(buf + 34) = 16;
    memcpy(buf + 36, "data", 4);
    *(uint32_t *)(buf + 40) = (uint32_t)dataLen;

    int16_t *pcm = (int16_t *)(buf + 44);
    double amp = 12000.0;
    for (int i = 0; i < nsamples; i++) {
        double env = 1.0 - (double)i / (double)nsamples;
        pcm[i] = (int16_t)(amp * env * sin(2.0 * 3.14159265358979 * hz * i / sampleRate));
    }

    *out = buf;
    *outlen = total;
}

static int h_blip = -1;
static int h_laser = -1;

static void draw_hud(void)
{
    int mx, my;
    io_get_mouse(&mx, &my);
    char line[128];
    snprintf(line, sizeof(line), "phase1 IO layer  mouse=%d,%d  A=blip W=laser S=pitch/2 ESC=quit", mx, my);
    DrawText(line, 8, 8, 16, YELLOW);
    DrawFPS(GetScreenWidth() - 80, 8);
}

int main(void)
{
    io_timer_init();
    io_video_init(LOGICAL_W, LOGICAL_H, WINDOW_SCALE, "DIV Port - Fase 1 (IO layer, raylib)");
    uint8_t *fb = io_framebuffer();
    if (!fb)
        return 1;

    uint8_t pal[768];
    for (int i = 0; i < 256; i++)
        hsv_to_rgb((float)i / 256.0f, 1.0f, 1.0f, &pal[i * 3], &pal[i * 3 + 1], &pal[i * 3 + 2]);
    io_palette_set(pal);

    if (io_audio_init() == 0) {
        uint8_t *wav;
        long wavlen;
        synth_wav(&wav, &wavlen, 22050 / 8, 330.0);
        h_blip = io_load_sound(wav, wavlen, 0);
        free(wav);
        synth_wav(&wav, &wavlen, 22050 / 10, 880.0);
        h_laser = io_load_sound(wav, wavlen, 0);
        free(wav);
    }

    io_frameclock_t fc;
    io_frameclock_set_fps(&fc, 60.0);
    io_frameclock_begin(&fc);

    double t = 0.0;
    float hue_shift = 0.0f;

    while (!WindowShouldClose() && !io_key_pressed(_ESC)) {
        io_poll();

        if (io_key_pressed(_A) && h_blip >= 0)
            io_play_sound(h_blip, 200, 256);
        if (io_key_pressed(_W) && h_laser >= 0)
            io_play_sound(h_laser, 200, 256);
        if (io_key_pressed(_S) && h_blip >= 0)
            io_play_sound(h_blip, 200, 128);

        hue_shift += 0.002f;
        if (hue_shift > 1.0f)
            hue_shift -= 1.0f;
        for (int i = 0; i < 256; i++) {
            float h = (float)i / 256.0f + hue_shift;
            if (h > 1.0f)
                h -= 1.0f;
            hsv_to_rgb(h, 1.0f, 1.0f, &pal[i * 3], &pal[i * 3 + 1], &pal[i * 3 + 2]);
        }
        io_palette_set(pal);

        t += io_frameclock_elapsed(&fc);

        for (int y = 0; y < LOGICAL_H; y++) {
            for (int x = 0; x < LOGICAL_W; x++) {
                double f = sin((double)x * 0.045 + t * 2.2) + cos((double)y * 0.035 + t * 1.7) +
                           sin(((double)x + (double)y) * 0.02 + t * 1.2);
                fb[y * LOGICAL_W + x] = (uint8_t)((f + 3.0) * 0.25 * 255.0);
            }
        }

        int mx, my;
        io_get_mouse(&mx, &my);
        for (int i = -6; i <= 6; i++) {
            int cx = mx + i, cy = my + i;
            if (cx >= 0 && cx < LOGICAL_W)
                fb[my * LOGICAL_W + cx] = 250;
            if (cy >= 0 && cy < LOGICAL_H)
                fb[cy * LOGICAL_W + mx] = 250;
        }

        io_present_ui(draw_hud);

        io_frameclock_wait(&fc);
        io_frameclock_begin(&fc);
    }

    io_audio_close();
    io_video_close();

    return 0;
}