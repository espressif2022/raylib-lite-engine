// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAYLIB_LITE_UI_CAPACITY 32
#define RAYLIB_LITE_UI_POINTER_CAPACITY 2

typedef enum {
    RAYLIB_LITE_UI_PANEL,
    RAYLIB_LITE_UI_LABEL,
    RAYLIB_LITE_UI_BUTTON,
} raylib_lite_ui_type_t;
typedef void (*raylib_lite_ui_click_cb_t)(uint16_t id, void *context);
typedef struct {
    uint16_t id;
    raylib_lite_ui_type_t type;
    raylib_lite_renderer_rect_t bounds;
    const char *text;
    raylib_lite_renderer_color_t color;
    raylib_lite_renderer_color_t text_color;
    bool visible, pressed;
    raylib_lite_ui_click_cb_t click;
    void *context;
} raylib_lite_ui_node_t;
typedef struct { int32_t track_id; float x, y; bool active; } raylib_lite_ui_pointer_t;
typedef struct {
    raylib_lite_ui_node_t nodes[RAYLIB_LITE_UI_CAPACITY];
    size_t count;
    int focused;
    raylib_lite_ui_pointer_t pointers[RAYLIB_LITE_UI_POINTER_CAPACITY];
    const void *input_dispatch; /* internal; do not modify or copy during callbacks */
} raylib_lite_ui_t;

void raylib_lite_ui_init(raylib_lite_ui_t *ui);
bool raylib_lite_ui_add(raylib_lite_ui_t *ui, raylib_lite_ui_node_t node);
/* pressed=true is a down/move sample; pressed=false releases the track.
 * Hit bounds are half-open. A release clicks each visible button containing
 * both the release and previous sample (not necessarily the original down).
 * Two tracks are supported; releasing one preserves the other track.
 * State is committed before callbacks. Reinitializing, adding a node, cancelling
 * input, or recursively dispatching input stops the previous event dispatch.
 * Other node edits are observed before subsequent callbacks; callbacks must
 * keep the ui object alive until this call returns. */
bool raylib_lite_ui_pointer(raylib_lite_ui_t *ui, int32_t track_id,
                            float x, float y, bool pressed);
/* Direction sign moves one visible button forward/backward with wrapping.
 * Zero leaves focus unchanged; confirming requires a currently visible button.
 * Invalid/hidden focus is cleared. No callback runs on failed selection. */
bool raylib_lite_ui_action(raylib_lite_ui_t *ui, int direction, bool confirm);
/* Drop all active pointers, focus and pressed flags without invoking clicks.
 * Use on scene changes, focus loss or a platform pointer-cancel event. */
void raylib_lite_ui_cancel_input(raylib_lite_ui_t *ui);
void raylib_lite_ui_draw(raylib_lite_ui_t *ui, int font_size);

#ifdef __cplusplus
}
#endif
