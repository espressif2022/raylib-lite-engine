// SPDX-License-Identifier: Apache-2.0
#include "mosaico_rgb565.h"
#include <string.h>

uint8_t mosaico_shade_r_lut[16][32];
uint8_t mosaico_shade_g_lut[16][64];
uint8_t mosaico_shade_b_lut[16][32];

void mosaico_shade_lut_init(void)
{
    static int ready;
    if (ready) return;
    for (int level = 0; level < 16; ++level) {
        unsigned light = (unsigned)level << 4;
        for (int c = 0; c < 32; ++c) {
            mosaico_shade_r_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
            mosaico_shade_b_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
        }
        for (int c = 0; c < 64; ++c)
            mosaico_shade_g_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
    }
    ready = 1;
}

#if defined(__GNUC__)
static void mosaico_shade_lut_ctor(void) __attribute__((constructor));
static void mosaico_shade_lut_ctor(void) { mosaico_shade_lut_init(); }
#endif

#if defined(MOSAICO_RGB565_PIE)
void mosaico_rgb565_copy_pie(uint16_t *dst, const uint16_t *src, size_t count);
void mosaico_rgb565_fill_pie(uint16_t *dst, uint16_t color, size_t count);
#endif

void mosaico_copy_rgb565(uint16_t *dst, const uint16_t *src, size_t count)
{
    if (!dst || !src || count == 0) {
        return;
    }
#if defined(MOSAICO_RGB565_PIE)
    /* PIE is worth it from 16 pixels; 8-wide walls stay scalar. */
    if (count >= 16U) {
        size_t n = count & ~(size_t)7U;
        mosaico_rgb565_copy_pie(dst, src, n);
        dst += n;
        src += n;
        count -= n;
    }
#endif
    if (count) {
        memcpy(dst, src, count * sizeof(uint16_t));
    }
}

void mosaico_fill_rgb565(uint16_t *dst, uint16_t color, size_t count)
{
    if (!dst || count == 0) {
        return;
    }
#if defined(MOSAICO_RGB565_PIE)
    if (count >= 16U) {
        size_t n = count & ~(size_t)7U;
        mosaico_rgb565_fill_pie(dst, color, n);
        dst += n;
        count -= n;
    }
#endif
    if (count == 2) {
        dst[0] = dst[1] = color;
        return;
    }
    if (count == 4) {
        dst[0] = dst[1] = dst[2] = dst[3] = color;
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        dst[i] = color;
    }
}

void mosaico_shade_rgb565(uint16_t *dst, const uint16_t *src, size_t count,
                          unsigned light256)
{
    mosaico_shade_lut_init();
    if (!dst || !src || count == 0) {
        return;
    }
    if (light256 >= 256U) {
        mosaico_copy_rgb565(dst, src, count);
        return;
    }
    size_t i = 0;
    for (; i + 7U < count; i += 8U) {
        dst[i] = mosaico_shade565(src[i], light256);
        dst[i + 1] = mosaico_shade565(src[i + 1], light256);
        dst[i + 2] = mosaico_shade565(src[i + 2], light256);
        dst[i + 3] = mosaico_shade565(src[i + 3], light256);
        dst[i + 4] = mosaico_shade565(src[i + 4], light256);
        dst[i + 5] = mosaico_shade565(src[i + 5], light256);
        dst[i + 6] = mosaico_shade565(src[i + 6], light256);
        dst[i + 7] = mosaico_shade565(src[i + 7], light256);
    }
    for (; i < count; ++i) {
        dst[i] = mosaico_shade565(src[i], light256);
    }
}
