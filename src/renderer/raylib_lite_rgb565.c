// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_rgb565.h"
#include <string.h>

uint8_t raylib_lite_rgb565_shade_r_lut[16][32];
uint8_t raylib_lite_rgb565_shade_g_lut[16][64];
uint8_t raylib_lite_rgb565_shade_b_lut[16][32];

void raylib_lite_rgb565_shade_lut_init(void)
{
    static int ready;
    if (ready) return;
    for (int level = 0; level < 16; ++level) {
        unsigned light = (unsigned)level << 4;
        for (int c = 0; c < 32; ++c) {
            raylib_lite_rgb565_shade_r_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
            raylib_lite_rgb565_shade_b_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
        }
        for (int c = 0; c < 64; ++c)
            raylib_lite_rgb565_shade_g_lut[level][c] = (uint8_t)((unsigned)c * light >> 8);
    }
    ready = 1;
}

#if defined(__GNUC__)
static void raylib_lite_rgb565_shade_lut_ctor(void) __attribute__((constructor));
static void raylib_lite_rgb565_shade_lut_ctor(void) { raylib_lite_rgb565_shade_lut_init(); }
#endif

#if defined(MOSAICO_RGB565_PIE)
void raylib_lite_rgb565_copy_pie(uint16_t *dst, const uint16_t *src, size_t count);
void raylib_lite_rgb565_fill_pie(uint16_t *dst, uint16_t color, size_t count);
#endif

void raylib_lite_rgb565_copy(uint16_t *dst, const uint16_t *src, size_t count)
{
    if (!dst || !src || count == 0) {
        return;
    }
#if defined(MOSAICO_RGB565_PIE)
    /* PIE is worth it from 16 pixels; 8-wide walls stay scalar. */
    if (count >= 16U) {
        size_t n = count & ~(size_t)7U;
        raylib_lite_rgb565_copy_pie(dst, src, n);
        dst += n;
        src += n;
        count -= n;
    }
#endif
    if (count) {
        memcpy(dst, src, count * sizeof(uint16_t));
    }
}

void raylib_lite_rgb565_fill(uint16_t *dst, uint16_t color, size_t count)
{
    if (!dst || count == 0) {
        return;
    }
#if defined(MOSAICO_RGB565_PIE)
    if (count >= 16U) {
        size_t n = count & ~(size_t)7U;
        raylib_lite_rgb565_fill_pie(dst, color, n);
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

void raylib_lite_rgb565_shade(uint16_t *dst, const uint16_t *src, size_t count,
                          unsigned light256)
{
    raylib_lite_rgb565_shade_lut_init();
    if (!dst || !src || count == 0) {
        return;
    }
    if (light256 >= 256U) {
        raylib_lite_rgb565_copy(dst, src, count);
        return;
    }
    size_t i = 0;
    for (; i + 7U < count; i += 8U) {
        dst[i] = raylib_lite_rgb565_shade_pixel(src[i], light256);
        dst[i + 1] = raylib_lite_rgb565_shade_pixel(src[i + 1], light256);
        dst[i + 2] = raylib_lite_rgb565_shade_pixel(src[i + 2], light256);
        dst[i + 3] = raylib_lite_rgb565_shade_pixel(src[i + 3], light256);
        dst[i + 4] = raylib_lite_rgb565_shade_pixel(src[i + 4], light256);
        dst[i + 5] = raylib_lite_rgb565_shade_pixel(src[i + 5], light256);
        dst[i + 6] = raylib_lite_rgb565_shade_pixel(src[i + 6], light256);
        dst[i + 7] = raylib_lite_rgb565_shade_pixel(src[i + 7], light256);
    }
    for (; i < count; ++i) {
        dst[i] = raylib_lite_rgb565_shade_pixel(src[i], light256);
    }
}
