// SPDX-License-Identifier: Apache-2.0
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "raylib_lite_clock.h"

#if defined(_WIN32)
#include <windows.h>
uint64_t raylib_lite_time_us(void)
{
    LARGE_INTEGER counter, frequency;
    if (!QueryPerformanceCounter(&counter) ||
        !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        return 0;
    uint64_t ticks = (uint64_t)counter.QuadPart;
    uint64_t hz = (uint64_t)frequency.QuadPart;
    return (ticks / hz) * 1000000U + (ticks % hz) * 1000000U / hz;
}
#else
#include <time.h>
uint64_t raylib_lite_time_us(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000000U + (uint64_t)now.tv_nsec / 1000U;
}
#endif
