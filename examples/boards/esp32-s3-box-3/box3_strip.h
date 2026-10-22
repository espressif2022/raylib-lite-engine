// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Pure nearest-neighbor RGB565 strip extraction for the BOX-3 SPI presenter.
 * Kept independent from ESP-IDF so Host tests check the actual pixel path. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "box3_viewport.h"

/* Samples one viewport row; the rational step gives exactly
 * floor(x * logical_w / view_width), including upscales, without a division
 * per pixel. */
static inline void box3_sample_view_row(
    const box3_viewport_t *view, const uint16_t *frame, uint16_t *out,
    uint32_t view_row, bool swap_bytes)
{
    const uint32_t logical_w = view->logical_width;
    if (!swap_bytes && logical_w == view->view_width &&
            view->logical_height == view->view_height) {
        memcpy(out, frame + view_row * logical_w, logical_w * sizeof(uint16_t));
        return;
    }
    const uint32_t base = logical_w / view->view_width;
    const uint32_t remainder = logical_w % view->view_width;
    uint32_t src_y = (view_row * view->logical_height) / view->view_height;
    const uint16_t *src = frame + src_y * logical_w;
    uint32_t source_x = 0, error = 0;
    for (uint32_t x = 0; x < view->view_width; ++x) {
        uint16_t color = src[source_x];
        if (swap_bytes) color = (uint16_t)((color << 8) | (color >> 8));
        out[x] = color;
        source_x += base;
        error += remainder;
        if (error >= view->view_width) {
            error -= view->view_width;
            ++source_x;
        }
    }
}

/* Full LCD-width rows, including the black letterbox bars. */
static inline void box3_fill_strip_pixels(
    const box3_viewport_t *view, const uint16_t *frame, uint16_t *strip,
    uint16_t top, uint16_t count, bool swap_bytes)
{
    for (uint32_t row = 0; row < count; row++) {
        uint32_t y = (uint32_t)top + row;
        uint16_t *dst = strip + row * view->lcd_width;
        if (y < view->view_y ||
                y >= (uint32_t)view->view_y + view->view_height) {
            memset(dst, 0, view->lcd_width * sizeof(uint16_t));
            continue;
        }
        memset(dst, 0, view->view_x * sizeof(uint16_t));
        memset(dst + view->view_x + view->view_width, 0,
               (view->lcd_width-view->view_x-view->view_width)*sizeof(uint16_t));
        box3_sample_view_row(view, frame, dst + view->view_x,
                             y - view->view_y, swap_bytes);
    }
}

/* Packed viewport-only rows (view_width pixels each); `top` is relative to
 * the viewport. Used once the bars are known to be on the panel. */
static inline void box3_fill_view_pixels(
    const box3_viewport_t *view, const uint16_t *frame, uint16_t *strip,
    uint16_t top, uint16_t count, bool swap_bytes)
{
    for (uint32_t row = 0; row < count; row++)
        box3_sample_view_row(view, frame, strip + row * view->view_width,
                             (uint32_t)top + row, swap_bytes);
}
