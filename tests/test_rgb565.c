// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mosaico_rgb565.h"

static uint16_t shade_mul(uint16_t pixel, unsigned light)
{
    if (light >= 256U) return pixel;
    unsigned r = ((pixel >> 11) & 31U) * light >> 8;
    unsigned g = ((pixel >> 5) & 63U) * light >> 8;
    unsigned b = (pixel & 31U) * light >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

int main(void)
{
    mosaico_shade_lut_init();
    mosaico_shade_lut_init();

    uint16_t dst[24];
    uint16_t src[24];
    for (int i = 0; i < 24; ++i) src[i] = (uint16_t)(i * 997 + 123);

    memset(dst, 0x3c, sizeof(dst));
    mosaico_fill_rgb565(dst, 0x8410, 24);
    for (int i = 0; i < 24; ++i) assert(dst[i] == 0x8410);

    memset(dst, 0, sizeof(dst));
    mosaico_copy_rgb565(dst, src, 24);
    assert(!memcmp(dst, src, sizeof(src)));

    mosaico_copy_rgb565(NULL, src, 8);
    mosaico_fill_rgb565(NULL, 1, 8);
    mosaico_shade_rgb565(NULL, src, 8, 128);

    for (int level = 0; level < 16; ++level) {
        unsigned light = (unsigned)level << 4;
        for (int c = 0; c < 32; ++c) {
            assert(mosaico_shade_r_lut[level][c] == (uint8_t)((unsigned)c * light >> 8));
            assert(mosaico_shade_b_lut[level][c] == (uint8_t)((unsigned)c * light >> 8));
        }
        for (int c = 0; c < 64; ++c)
            assert(mosaico_shade_g_lut[level][c] == (uint8_t)((unsigned)c * light >> 8));
    }

    static const unsigned lights[] = {0, 1, 15, 16, 17, 128, 160, 240, 255, 256, 300};
    for (size_t li = 0; li < sizeof(lights) / sizeof(lights[0]); ++li) {
        unsigned light = lights[li];
        for (unsigned p = 0; p < 65536U; p += 17U)
            assert(mosaico_shade565((uint16_t)p, light) == shade_mul((uint16_t)p, light));
        mosaico_shade_rgb565(dst, src, 24, light);
        for (int i = 0; i < 24; ++i)
            assert(dst[i] == shade_mul(src[i], light));
    }

    puts("RGB565 fill/copy/shade LUT oracle passed");
    return 0;
}
