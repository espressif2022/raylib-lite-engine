// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>

/* 8-wide RGB565 primitives. Device (ESP32-S31 PIE) and Host share the
 * same pixel results. Short runs stay on the scalar path. */
void mosaico_fill_rgb565(uint16_t *dst, uint16_t color, size_t count);
void mosaico_copy_rgb565(uint16_t *dst, const uint16_t *src, size_t count);
void mosaico_shade_rgb565(uint16_t *dst, const uint16_t *src, size_t count,
                          unsigned light256);

static inline uint16_t mosaico_shade565(uint16_t pixel, unsigned light)
{
    if (light >= 256U) {
        return pixel;
    }
    unsigned r = ((pixel >> 11) & 31U) * light >> 8;
    unsigned g = ((pixel >> 5) & 63U) * light >> 8;
    unsigned b = (pixel & 31U) * light >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
