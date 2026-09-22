// SPDX-License-Identifier: Apache-2.0
/* Host microbenchmark only; not a device frame-rate estimate. */
#include <stdio.h>
#include <time.h>
#include "mosaico_rgb565.h"

uint8_t mosaico_shade_r_lut[16][32];
uint8_t mosaico_shade_g_lut[16][64];
uint8_t mosaico_shade_b_lut[16][32];

static inline uint16_t reference_shade(uint16_t pixel, unsigned light)
{
    if (light >= 256U) return pixel;
    unsigned level = light >> 4;
    return (uint16_t)((mosaico_shade_r_lut[level][(pixel >> 11) & 31U] << 11) |
                      (mosaico_shade_g_lut[level][(pixel >> 5) & 63U] << 5) |
                      mosaico_shade_b_lut[level][pixel & 31U]);
}

#ifdef SHADE_BASELINE
#define shade reference_shade
#else
#define shade mosaico_shade565
#endif

static volatile unsigned lights[] = {16, 64, 128, 160, 240};

int main(void)
{
    for (unsigned level = 0; level < 16; ++level) {
        for (unsigned c = 0; c < 32; ++c)
            mosaico_shade_r_lut[level][c] = mosaico_shade_b_lut[level][c] = c * level >> 4;
        for (unsigned c = 0; c < 64; ++c)
            mosaico_shade_g_lut[level][c] = c * level >> 4;
    }
    unsigned sum = 0;
    clock_t start = clock();
    for (unsigned repeat = 0; repeat < 2000; ++repeat)
        for (unsigned pixel = 0; pixel < 65536; ++pixel)
            sum += shade((uint16_t)pixel, lights[repeat % 5]);
    printf("%.3f ms checksum=%u\n", 1000.0 * (clock() - start) / CLOCKS_PER_SEC, sum);
    return 0;
}
