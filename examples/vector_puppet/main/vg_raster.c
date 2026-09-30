// SPDX-License-Identifier: Apache-2.0
#include "vg_raster.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mosaico_rgb565.h"

#define VG_SUBROWS 4
#define VG_SUBPIXEL 16
#define VG_FULL (VG_SUBROWS * VG_SUBPIXEL)
#define VG_MAX_CROSSINGS 256

typedef struct {
    float x0, y0, y1, dxdy;
    int dir;
} vg_edge_t;

typedef struct {
    int x;
    int dir;
} vg_crossing_t;

static uint16_t *s_pixels;
static int s_width, s_height;
static size_t s_stride;
static vg_mat_t s_matrix = {1, 0, 0, 1, 0, 0};
static vg_stats_t s_stats;

static vg_edge_t s_edges[VG_MAX_EDGES];
static int s_edge_count;
static float s_min_x, s_min_y, s_max_x, s_max_y;
static float s_cur_x, s_cur_y, s_start_x, s_start_y;
static int s_open;

static int16_t s_acc[VG_MAX_WIDTH + 4];
static int s_active[VG_MAX_EDGES];
static vg_crossing_t s_cross[VG_MAX_CROSSINGS];

void vg_begin(uint16_t *pixels, int width, int height, size_t stride)
{
    s_pixels = pixels;
    s_width = width > VG_MAX_WIDTH ? VG_MAX_WIDTH : width;
    s_height = height;
    s_stride = stride;
    s_matrix = vg_identity();
}

void vg_reset_stats(void) { memset(&s_stats, 0, sizeof(s_stats)); }
vg_stats_t vg_get_stats(void) { return s_stats; }

vg_mat_t vg_identity(void) { return (vg_mat_t){1, 0, 0, 1, 0, 0}; }

vg_mat_t vg_mul(vg_mat_t p, vg_mat_t c)
{
    return (vg_mat_t){
        p.a * c.a + p.c * c.b, p.b * c.a + p.d * c.b,
        p.a * c.c + p.c * c.d, p.b * c.c + p.d * c.d,
        p.a * c.e + p.c * c.f + p.e, p.b * c.e + p.d * c.f + p.f,
    };
}

vg_mat_t vg_translate(vg_mat_t m, float x, float y)
{
    return vg_mul(m, (vg_mat_t){1, 0, 0, 1, x, y});
}

vg_mat_t vg_rotate(vg_mat_t m, float radians)
{
    float c = cosf(radians), s = sinf(radians);
    return vg_mul(m, (vg_mat_t){c, s, -s, c, 0, 0});
}

vg_mat_t vg_scale(vg_mat_t m, float sx, float sy)
{
    return vg_mul(m, (vg_mat_t){sx, 0, 0, sy, 0, 0});
}

void vg_set_matrix(vg_mat_t m) { s_matrix = m; }
vg_mat_t vg_get_matrix(void) { return s_matrix; }

void vg_apply(vg_mat_t m, float x, float y, float *ox, float *oy)
{
    *ox = m.a * x + m.c * y + m.e;
    *oy = m.b * x + m.d * y + m.f;
}

static void add_edge(float x0, float y0, float x1, float y1)
{
    if (y0 == y1 || s_edge_count >= VG_MAX_EDGES) return;
    int dir = 1;
    if (y0 > y1) {
        float t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
        dir = -1;
    }
    vg_edge_t *e = &s_edges[s_edge_count++];
    e->x0 = x0;
    e->y0 = y0;
    e->y1 = y1;
    e->dxdy = (x1 - x0) / (y1 - y0);
    e->dir = dir;
    if (x0 < s_min_x) s_min_x = x0;
    if (x1 < s_min_x) s_min_x = x1;
    if (x0 > s_max_x) s_max_x = x0;
    if (x1 > s_max_x) s_max_x = x1;
    if (y0 < s_min_y) s_min_y = y0;
    if (y1 > s_max_y) s_max_y = y1;
}

static void screen_line_to(float x, float y)
{
    add_edge(s_cur_x, s_cur_y, x, y);
    s_cur_x = x;
    s_cur_y = y;
}

void vg_path_begin(void)
{
    s_edge_count = 0;
    s_open = 0;
    s_min_x = s_min_y = 1e9f;
    s_max_x = s_max_y = -1e9f;
}

void vg_close(void)
{
    if (!s_open) return;
    screen_line_to(s_start_x, s_start_y);
    s_open = 0;
}

void vg_move_to(float x, float y)
{
    vg_close();
    vg_apply(s_matrix, x, y, &s_cur_x, &s_cur_y);
    s_start_x = s_cur_x;
    s_start_y = s_cur_y;
    s_open = 1;
}

void vg_line_to(float x, float y)
{
    float sx, sy;
    vg_apply(s_matrix, x, y, &sx, &sy);
    screen_line_to(sx, sy);
}

void vg_cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y)
{
    float x0 = s_cur_x, y0 = s_cur_y, x1, y1, x2, y2, x3, y3;
    vg_apply(s_matrix, c1x, c1y, &x1, &y1);
    vg_apply(s_matrix, c2x, c2y, &x2, &y2);
    vg_apply(s_matrix, x, y, &x3, &y3);
    float ddx = fabsf(x0 - 2 * x1 + x2) + fabsf(x1 - 2 * x2 + x3);
    float ddy = fabsf(y0 - 2 * y1 + y2) + fabsf(y1 - 2 * y2 + y3);
    int n = (int)ceilf(sqrtf((ddx + ddy) * 0.75f));
    if (n < 1) n = 1;
    if (n > 40) n = 40;
    for (int i = 1; i <= n; ++i) {
        float t = (float)i / (float)n, u = 1 - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        screen_line_to(a * x0 + b * x1 + c * x2 + d * x3,
                       a * y0 + b * y1 + c * y2 + d * y3);
    }
}

void vg_quad_to(float cx, float cy, float x, float y)
{
    /* The current point is already in screen space, so elevate there. */
    float sx, sy, ex, ey;
    vg_apply(s_matrix, cx, cy, &sx, &sy);
    vg_apply(s_matrix, x, y, &ex, &ey);
    float x0 = s_cur_x, y0 = s_cur_y;
    float c1x = x0 + (sx - x0) * (2.0f / 3.0f), c1y = y0 + (sy - y0) * (2.0f / 3.0f);
    float c2x = ex + (sx - ex) * (2.0f / 3.0f), c2y = ey + (sy - ey) * (2.0f / 3.0f);
    vg_mat_t saved = s_matrix;
    s_matrix = vg_identity();
    vg_cubic_to(c1x, c1y, c2x, c2y, ex, ey);
    s_matrix = saved;
}

void vg_ellipse(float cx, float cy, float rx, float ry)
{
    const float k = 0.5522848f;
    vg_move_to(cx + rx, cy);
    vg_cubic_to(cx + rx, cy + ry * k, cx + rx * k, cy + ry, cx, cy + ry);
    vg_cubic_to(cx - rx * k, cy + ry, cx - rx, cy + ry * k, cx - rx, cy);
    vg_cubic_to(cx - rx, cy - ry * k, cx - rx * k, cy - ry, cx, cy - ry);
    vg_cubic_to(cx + rx * k, cy - ry, cx + rx, cy - ry * k, cx + rx, cy);
    vg_close();
}

void vg_round_rect(float x, float y, float w, float h, float r)
{
    const float k = 0.5522848f * r;
    vg_move_to(x + r, y);
    vg_line_to(x + w - r, y);
    vg_cubic_to(x + w - r + k, y, x + w, y + r - k, x + w, y + r);
    vg_line_to(x + w, y + h - r);
    vg_cubic_to(x + w, y + h - r + k, x + w - r + k, y + h, x + w - r, y + h);
    vg_line_to(x + r, y + h);
    vg_cubic_to(x + r - k, y + h, x, y + h - r + k, x, y + h - r);
    vg_line_to(x, y + r);
    vg_cubic_to(x, y + r - k, x + r - k, y, x + r, y);
    vg_close();
}

static int edge_cmp(const void *a, const void *b)
{
    float ya = ((const vg_edge_t *)a)->y0, yb = ((const vg_edge_t *)b)->y0;
    return (ya > yb) - (ya < yb);
}

static inline uint16_t blend565(uint16_t dst, uint16_t src, unsigned a32)
{
    uint32_t fg = ((uint32_t)src | ((uint32_t)src << 16)) & 0x07e0f81fU;
    uint32_t bg = ((uint32_t)dst | ((uint32_t)dst << 16)) & 0x07e0f81fU;
    bg += ((fg - bg) * a32) >> 5;
    bg &= 0x07e0f81fU;
    return (uint16_t)(bg | (bg >> 16));
}

static inline void add_span(int a, int b, int lo, int hi, int *row_min, int *row_max)
{
    if (a < lo) a = lo;
    if (b > hi) b = hi;
    if (b <= a) return;
    int ia = a >> 4, fa = a & 15, ib = b >> 4, fb = b & 15;
    s_acc[ia] += (int16_t)(VG_SUBPIXEL - fa);
    s_acc[ia + 1] += (int16_t)fa;
    s_acc[ib] -= (int16_t)(VG_SUBPIXEL - fb);
    s_acc[ib + 1] -= (int16_t)fb;
    if (ia < *row_min) *row_min = ia;
    if (ib + 1 > *row_max) *row_max = ib + 1;
}

void vg_fill(uint16_t color, uint8_t alpha)
{
    vg_close();
    if (!s_pixels || s_edge_count == 0 || alpha == 0) return;
    int iy0 = (int)floorf(s_min_y), iy1 = (int)ceilf(s_max_y);
    int ix0 = (int)floorf(s_min_x), ix1 = (int)floorf(s_max_x);
    if (iy0 < 0) iy0 = 0;
    if (iy1 > s_height) iy1 = s_height;
    if (ix0 < 0) ix0 = 0;
    if (ix1 > s_width - 1) ix1 = s_width - 1;
    if (iy0 >= iy1 || ix0 > ix1) return;

    ++s_stats.fills;
    s_stats.edges += (uint32_t)s_edge_count;
    qsort(s_edges, (size_t)s_edge_count, sizeof(s_edges[0]), edge_cmp);

    const int lo = ix0 * VG_SUBPIXEL, hi = (ix1 + 1) * VG_SUBPIXEL;
    int next = 0, active = 0;
    for (int py = iy0; py < iy1; ++py) {
        int row_min = VG_MAX_WIDTH + 2, row_max = -1;
        for (int k = 0; k < VG_SUBROWS; ++k) {
            float sy = (float)py + ((float)k + 0.5f) * (1.0f / VG_SUBROWS);
            while (next < s_edge_count && s_edges[next].y0 <= sy) {
                if (s_edges[next].y1 > sy) s_active[active++] = next;
                ++next;
            }
            int count = 0, keep = 0;
            for (int i = 0; i < active; ++i) {
                const vg_edge_t *e = &s_edges[s_active[i]];
                if (e->y1 <= sy) continue;
                s_active[keep++] = s_active[i];
                if (count >= VG_MAX_CROSSINGS) continue;
                float x = e->x0 + (sy - e->y0) * e->dxdy;
                vg_crossing_t c = {(int)lrintf(x * VG_SUBPIXEL), e->dir};
                int j = count++;
                while (j > 0 && s_cross[j - 1].x > c.x) {
                    s_cross[j] = s_cross[j - 1];
                    --j;
                }
                s_cross[j] = c;
            }
            active = keep;
            int wind = 0, start = 0;
            for (int i = 0; i < count; ++i) {
                int before = wind;
                wind += s_cross[i].dir;
                if (before == 0 && wind != 0) start = s_cross[i].x;
                else if (before != 0 && wind == 0)
                    add_span(start, s_cross[i].x, lo, hi, &row_min, &row_max);
            }
        }
        if (row_max < row_min) continue;

        uint16_t *row = s_pixels + (size_t)py * s_stride;
        int cover = 0, run_start = -1;
        for (int x = row_min; x <= row_max; ++x) {
            cover += s_acc[x];
            s_acc[x] = 0;
            if (x > ix1) continue;
            if (cover >= VG_FULL && alpha == 255) {
                if (run_start < 0) run_start = x;
                continue;
            }
            if (run_start >= 0) {
                mosaico_fill_rgb565(row + run_start, color, (size_t)(x - run_start));
                s_stats.solid_pixels += (uint32_t)(x - run_start);
                run_start = -1;
            }
            if (cover <= 0) continue;
            unsigned a32 = (unsigned)(cover > VG_FULL ? VG_FULL : cover) * alpha / 510U;
            if (a32) {
                row[x] = blend565(row[x], color, a32);
                ++s_stats.edge_pixels;
            }
        }
        if (run_start >= 0) {
            int end = row_max > ix1 ? ix1 + 1 : row_max + 1;
            mosaico_fill_rgb565(row + run_start, color, (size_t)(end - run_start));
            s_stats.solid_pixels += (uint32_t)(end - run_start);
        }
    }
    s_edge_count = 0;
}

void vg_stroke_cubic(float x0, float y0, float x1, float y1, float x2, float y2,
                     float x3, float y3, float w0, float w1, uint16_t color,
                     uint8_t alpha)
{
    enum { N = 16 };
    float px[N + 1], py[N + 1];
    for (int i = 0; i <= N; ++i) {
        float t = (float)i / N, u = 1 - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        px[i] = a * x0 + b * x1 + c * x2 + d * x3;
        py[i] = a * y0 + b * y1 + c * y2 + d * y3;
    }
    float lx[N + 1], ly[N + 1], rx[N + 1], ry[N + 1];
    for (int i = 0; i <= N; ++i) {
        int i0 = i > 0 ? i - 1 : 0, i1 = i < N ? i + 1 : N;
        float tx = px[i1] - px[i0], ty = py[i1] - py[i0];
        float len = sqrtf(tx * tx + ty * ty);
        if (len < 1e-4f) { tx = 1; ty = 0; len = 1; }
        float t = (float)i / N;
        float hw = 0.5f * (w0 + (w1 - w0) * t);
        float nx = -ty / len * hw, ny = tx / len * hw;
        lx[i] = px[i] + nx; ly[i] = py[i] + ny;
        rx[i] = px[i] - nx; ry[i] = py[i] - ny;
    }
    vg_path_begin();
    vg_move_to(lx[0], ly[0]);
    for (int i = 1; i <= N; ++i) vg_line_to(lx[i], ly[i]);
    for (int i = N; i >= 0; --i) vg_line_to(rx[i], ry[i]);
    vg_fill(color, alpha);
}

void vg_stroke_line(float x0, float y0, float x1, float y1, float w,
                    uint16_t color, uint8_t alpha)
{
    float tx = x1 - x0, ty = y1 - y0, len = sqrtf(tx * tx + ty * ty);
    if (len < 1e-4f) return;
    float nx = -ty / len * w * 0.5f, ny = tx / len * w * 0.5f;
    vg_path_begin();
    vg_move_to(x0 + nx, y0 + ny);
    vg_line_to(x1 + nx, y1 + ny);
    vg_line_to(x1 - nx, y1 - ny);
    vg_line_to(x0 - nx, y0 - ny);
    vg_fill(color, alpha);
}
