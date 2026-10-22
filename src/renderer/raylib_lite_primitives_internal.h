// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Internal screen-space primitive raster contract; not a public game API.
 * The caller owns the buffer, half-open clip and counter storage. No camera,
 * runtime, Raylib types or per-draw allocations belong in these kernels. */
#include <stdint.h>
#include <string.h>
#include "raylib_lite_renderer_draw.h"
#include "raylib_lite_rgb565.h"

typedef struct {
    uint16_t *pixels;
    size_t stride;
    int width, height;
    int clip_x0, clip_y0, clip_x1, clip_y1;
    bool clip_active;
    /* Rows with (y & row_mask) set are never displayed; spans skip them. */
    int row_mask;
    uint32_t pixel_count, run_count;
} raylib_lite_primitive_surface_t;

typedef struct {
    uint16_t pixel;
    unsigned alpha, inverse, red, green, blue, red_blue;
} raylib_lite_primitive_paint_t;

static inline raylib_lite_primitive_paint_t raylib_lite_primitive_paint(
    raylib_lite_renderer_color_t c)
{
    uint16_t pixel=(uint16_t)(((uint16_t)(c.r & 0xf8U)<<8)|
                             ((uint16_t)(c.g & 0xfcU)<<3)|(c.b>>3));
    return (raylib_lite_primitive_paint_t){pixel,c.a,255U-c.a,
        c.r*c.a,c.g*c.a,c.b*c.a,((uint32_t)c.r*c.a<<16)|(c.b*c.a)};
}

static inline uint16_t raylib_lite_primitive_blend(unsigned old, const raylib_lite_primitive_paint_t *paint)
{
    /* Two independent 16-bit lanes. Each channel numerator is at
     * most 65025, so neither multiplication nor the exact /255
     * correction can carry into the adjacent channel. */
    uint32_t rb = (((old&0xf800U)<<8)|((old&31U)<<3))*paint->inverse + paint->red_blue;
    rb = (rb + 0x00010001U + ((rb>>8)&0x00ff00ffU))>>8;
    unsigned g = (((old>>5)&63U)<<2)*paint->inverse + paint->green;
    g = (g + 1U + (g>>8))>>8;
    return (uint16_t)(((rb&0x00f80000U)>>8)|((g&0xfcU)<<3)|((rb&0xf8U)>>3));
}

/* Translucent run of one paint; same result as per-pixel primitive_blend. */
void raylib_lite_primitive_blend_run(uint16_t *dst, int count,
    const raylib_lite_primitive_paint_t *paint);

/* Coordinates must already lie inside the target and clip; alpha is nonzero. */
static inline void raylib_lite_primitive_write_span(
    raylib_lite_primitive_surface_t *surface, int y, int x0, int x1,
    const raylib_lite_primitive_paint_t *paint)
{
    if (y & surface->row_mask) return;
    uint16_t *dst = surface->pixels + (size_t)y*surface->stride + x0;
    int count = x1-x0;
    surface->pixel_count+=(uint32_t)count;++surface->run_count;
    if (paint->alpha == 255) {
        /* Long opaque spans use the existing S31 eight-pixel PIE stores.
         * Keep tiny glyph and ray fragments on the inline scalar path. */
        if(count>=32){raylib_lite_rgb565_fill(dst,paint->pixel,(size_t)count);return;}
        uint16_t pixel = paint->pixel;
        uint32_t pair = (uint32_t)pixel | ((uint32_t)pixel << 16);
        if ((uintptr_t)dst & 3U) { *dst++ = pixel; --count; }
        while (count >= 2) { memcpy(dst, &pair, sizeof(pair)); dst += 2; count -= 2; }
        if (count) *dst = pixel;
    } else if (count >= 64) {
        raylib_lite_primitive_blend_run(dst, count, paint);
    } else {
        for (int i = 0; i < count; ++i) {
            dst[i] = raylib_lite_primitive_blend(dst[i], paint);
        }
    }
}

static inline void raylib_lite_primitive_fill_span(
    raylib_lite_primitive_surface_t *surface, int y, int x0, int x1,
    const raylib_lite_primitive_paint_t *paint)
{
    if (!surface->pixels || !paint->alpha || (unsigned)y >= (unsigned)surface->height) return;
    if (x0 < 0) x0 = 0;
    if (x1 > surface->width) x1 = surface->width;
    if (surface->clip_active) {
        if (y < surface->clip_y0 || y >= surface->clip_y1) return;
        if (x0 < surface->clip_x0) x0 = surface->clip_x0;
        if (x1 > surface->clip_x1) x1 = surface->clip_x1;
    }
    if (x0 >= x1) return;
    raylib_lite_primitive_write_span(surface,y,x0,x1,paint);
}


void raylib_lite_primitive_draw_rectangle(raylib_lite_primitive_surface_t *surface,
    int x, int y, int width, int height, raylib_lite_renderer_color_t color);
void raylib_lite_primitive_draw_triangle(raylib_lite_primitive_surface_t *surface,
    int ax, int ay, int bx, int by, int cx, int cy, raylib_lite_renderer_color_t color);

/* Out-of-line writer for complex coverage loops where inlining can increase
 * register pressure. Same checked span contract and counters as fill_span. */
void raylib_lite_primitive_draw_span(raylib_lite_primitive_surface_t *surface,
    int y, int x0, int x1, const raylib_lite_primitive_paint_t *paint);
