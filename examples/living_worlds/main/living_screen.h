// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Scenes are authored for a 480-unit-tall design view. Landscape panels widen
 * the span to living_view_width design units around the same centre and focal
 * length, so the vertical field of view holds. living_view_scale maps design
 * units to panel pixels; both default to the 480x480 identity. */
extern float living_view_width;
extern float living_view_scale;

#define LIVING_VIEW_W living_view_width
#define LIVING_VIEW_CX (living_view_width * .5f)
/* Left edge of the authored 480 span inside the widened view. */
#define LIVING_VIEW_OX ((living_view_width - 480.0f) * .5f)

#ifndef LIVING_WORLDS_SCENE_SIM_ONLY
#include "raylib_lite_2d.h"

/* Textured spans go straight to the rasterizer, outside Camera2D. */
static inline raylib_lite_textured_vertex_t living_view_px(raylib_lite_textured_vertex_t v)
{
    v.x *= living_view_scale;
    v.y *= living_view_scale;
    return v;
}

static inline void living_draw_triangle(Texture2D texture, raylib_lite_textured_vertex_t a,
    raylib_lite_textured_vertex_t b, raylib_lite_textured_vertex_t c, unsigned light256)
{
    if (living_view_scale != 1.0f) {
        a = living_view_px(a);
        b = living_view_px(b);
        c = living_view_px(c);
    }
    raylib_lite_2d_draw_textured_triangle(texture, a, b, c, light256);
}

static inline void living_draw_quad(Texture2D texture, raylib_lite_textured_vertex_t a,
    raylib_lite_textured_vertex_t b, raylib_lite_textured_vertex_t c,
    raylib_lite_textured_vertex_t d, unsigned light256)
{
    if (living_view_scale != 1.0f) {
        a = living_view_px(a);
        b = living_view_px(b);
        c = living_view_px(c);
        d = living_view_px(d);
    }
    raylib_lite_2d_draw_textured_quad(texture, a, b, c, d, light256);
}
#endif
