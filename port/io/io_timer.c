#include "div_io.h"

#include <windows.h>

static double io_qpc_scale = 0.0;

void io_timer_init(void)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    io_qpc_scale = (freq.QuadPart > 0) ? (1.0 / (double)freq.QuadPart) : 1e-7;
}

double io_time_now(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * io_qpc_scale;
}

uint64_t io_timer_micros(void)
{
    return (uint64_t)(io_time_now() * 1e6);
}

void io_frameclock_set_fps(io_frameclock_t *fc, double fps)
{
    if (fps <= 0.0)
        fps = 24.0;
    fc->frame_interval = 1.0 / fps;
}

void io_frameclock_begin(io_frameclock_t *fc)
{
    fc->frame_start = io_time_now();
}

void io_frameclock_wait(io_frameclock_t *fc)
{
    double target = fc->frame_start + fc->frame_interval;
    while (io_time_now() < target) {
        /* spin-wait identico al frame_start() original (que esperaba a que
         * el contador de 100 Hz alcanzara freloj) */
    }
}

double io_frameclock_elapsed(io_frameclock_t *fc)
{
    return io_time_now() - fc->frame_start;
}