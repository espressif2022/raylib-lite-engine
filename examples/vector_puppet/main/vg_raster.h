// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>

/*
 * Solid-fill vector rasterizer for RGB565 surfaces.
 *
 * Paths are flattened in screen space and filled with the non-zero rule.
 * Each pixel row takes four sub-scanlines; horizontal coverage is exact at
 * span ends. Interior pixels are written as plain runs, so anti-aliasing cost
 * follows the outline length instead of the filled area.
 */

#define VG_MAX_WIDTH 480
#define VG_MAX_EDGES 2048

typedef struct { float a, b, c, d, e, f; } vg_mat_t;

typedef struct {
    uint32_t fills;
    uint32_t edges;
    uint32_t solid_pixels;
    uint32_t edge_pixels;
} vg_stats_t;

static inline uint16_t vg_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint16_t)(((r & 0xf8U) << 8) | ((g & 0xfcU) << 3) | (b >> 3));
}

void vg_begin(uint16_t *pixels, int width, int height, size_t stride);
void vg_reset_stats(void);
vg_stats_t vg_get_stats(void);

vg_mat_t vg_identity(void);
vg_mat_t vg_mul(vg_mat_t parent, vg_mat_t child);
vg_mat_t vg_translate(vg_mat_t m, float x, float y);
vg_mat_t vg_rotate(vg_mat_t m, float radians);
vg_mat_t vg_scale(vg_mat_t m, float sx, float sy);
void vg_set_matrix(vg_mat_t m);
vg_mat_t vg_get_matrix(void);
void vg_apply(vg_mat_t m, float x, float y, float *ox, float *oy);

void vg_path_begin(void);
void vg_move_to(float x, float y);
void vg_line_to(float x, float y);
void vg_quad_to(float cx, float cy, float x, float y);
void vg_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y);
void vg_close(void);
void vg_ellipse(float cx, float cy, float rx, float ry);
void vg_round_rect(float x, float y, float w, float h, float r);
/* alpha 255 is opaque. Lower alpha is meant for small accents only. */
void vg_fill(uint16_t color, uint8_t alpha);

/* Tapered stroke of one cubic, filled as an outline polygon. */
void vg_stroke_cubic(float x0, float y0, float x1, float y1, float x2, float y2,
                     float x3, float y3, float w0, float w1, uint16_t color,
                     uint8_t alpha);
void vg_stroke_line(float x0, float y0, float x1, float y1, float w,
                    uint16_t color, uint8_t alpha);
