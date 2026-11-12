// SPDX-License-Identifier: Apache-2.0
#ifndef RAYLIB_LITE_DESIGN_VIEW_H
#define RAYLIB_LITE_DESIGN_VIEW_H

#include "raylib_lite_raylib.h"

/* Resolution-independent layout for example views. The short side of the
 * screen is always `design` units; the long side grows, so 480x480 maps 1:1
 * and a 320x240 panel becomes 640x480 units drawn at zoom 0.5. Layouts anchor
 * to `width`/`height` and use the extra length to show more world. */
typedef struct {
    float scale;
    float width, height;
} raylib_lite_design_view_t;

static inline raylib_lite_design_view_t raylib_lite_design_view(float design)
{
    int w = GetScreenWidth(), h = GetScreenHeight();
    int side = w < h ? w : h;
    float scale = side > 0 && design > 0 ? (float)side / design : 1.0f;
    if (w <= 0 || h <= 0) w = h = (int)design;
    return (raylib_lite_design_view_t){scale, (float)w / scale, (float)h / scale};
}

static inline bool raylib_lite_design_view_native(raylib_lite_design_view_t view)
{
    return view.scale > .999f && view.scale < 1.001f;
}

/* Wraps a design-space camera; the identity view keeps the caller's camera. */
static inline Camera2D raylib_lite_design_camera(raylib_lite_design_view_t view, Camera2D camera)
{
    if (camera.zoom == 0) camera.zoom = 1.0f;
    camera.offset.x *= view.scale;
    camera.offset.y *= view.scale;
    camera.zoom *= view.scale;
    return camera;
}

/* Starts design-space drawing. At 1:1 no camera is set, keeping the fast path. */
static inline void raylib_lite_design_begin(raylib_lite_design_view_t view)
{
    if (!raylib_lite_design_view_native(view))
        BeginMode2D((Camera2D){.zoom = view.scale});
}

static inline void raylib_lite_design_end(raylib_lite_design_view_t view)
{
    if (!raylib_lite_design_view_native(view)) EndMode2D();
}

/* Font size in design units for a crisp default font: on scaled views the
 * on-screen size snaps to whole multiples of its 10 px glyph, minimum 10 px. */
static inline int raylib_lite_design_font(raylib_lite_design_view_t view, int size)
{
    if (raylib_lite_design_view_native(view)) return size;
    int px = (int)((float)size * view.scale / 10.0f + .4f) * 10;
    if (px < 10) px = 10;
    return (int)((float)px / view.scale + .5f);
}

static inline Vector2 raylib_lite_design_point(raylib_lite_design_view_t view, float x, float y)
{
    return (Vector2){x / view.scale, y / view.scale};
}

/* Fixed play fields (a model with a fixed arena) are centred in the design
 * space; the margins on longer panels belong to the view. */
static inline Vector2 raylib_lite_design_field_origin(raylib_lite_design_view_t view,
                                                      float field_w, float field_h)
{
    return (Vector2){(view.width - field_w) * .5f, (view.height - field_h) * .5f};
}

static inline bool raylib_lite_design_field_identity(raylib_lite_design_view_t view,
                                                     float field_w, float field_h)
{
    Vector2 origin = raylib_lite_design_field_origin(view, field_w, field_h);
    return raylib_lite_design_view_native(view) && origin.x > -.5f && origin.x < .5f &&
           origin.y > -.5f && origin.y < .5f;
}

static inline void raylib_lite_design_field_begin(raylib_lite_design_view_t view,
                                                  float field_w, float field_h)
{
    if (raylib_lite_design_field_identity(view, field_w, field_h)) return;
    Vector2 origin = raylib_lite_design_field_origin(view, field_w, field_h);
    BeginMode2D((Camera2D){.offset = {origin.x * view.scale, origin.y * view.scale},
                           .zoom = view.scale});
}

static inline void raylib_lite_design_field_end(raylib_lite_design_view_t view,
                                                float field_w, float field_h)
{
    if (!raylib_lite_design_field_identity(view, field_w, field_h)) EndMode2D();
}

static inline Vector2 raylib_lite_design_field_point(raylib_lite_design_view_t view,
                                                     float field_w, float field_h,
                                                     float x, float y)
{
    Vector2 origin = raylib_lite_design_field_origin(view, field_w, field_h);
    return (Vector2){x / view.scale - origin.x, y / view.scale - origin.y};
}

/* Screen-space scissor for a design-space rectangle. */
static inline void raylib_lite_design_scissor(raylib_lite_design_view_t view,
                                              float x, float y, float w, float h)
{
    BeginScissorMode((int)(x * view.scale), (int)(y * view.scale),
                     (int)(w * view.scale + .999f), (int)(h * view.scale + .999f));
}

#endif
