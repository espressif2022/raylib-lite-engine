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
void mosaico_shade_lut_init(void);

extern uint8_t mosaico_shade_r_lut[16][32];
extern uint8_t mosaico_shade_g_lut[16][64];
extern uint8_t mosaico_shade_b_lut[16][32];

static inline uint16_t mosaico_shade565(uint16_t pixel, unsigned light)
{
    if (light >= 256U) {
        return pixel;
    }
    if ((light & 15U) == 0U) {
        /* Quantized light uses a four-bit factor. The red/blue products
         * fit in separate lanes, preserving exact channel truncation. */
        unsigned factor = light >> 4;
        uint32_t rb = (((uint32_t)(pixel & 0xf81fU) * factor) >> 4) & 0xf81fU;
        uint32_t g = (((uint32_t)(pixel & 0x07e0U) * factor) >> 4) & 0x07e0U;
        return (uint16_t)(rb | g);
    }
    unsigned r = ((pixel >> 11) & 31U) * light >> 8;
    unsigned g = ((pixel >> 5) & 63U) * light >> 8;
    unsigned b = (pixel & 31U) * light >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}
