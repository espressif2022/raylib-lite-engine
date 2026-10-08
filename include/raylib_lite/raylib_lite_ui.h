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
} raylib_lite_ui_t;

void raylib_lite_ui_init(raylib_lite_ui_t *ui);
bool raylib_lite_ui_add(raylib_lite_ui_t *ui, raylib_lite_ui_node_t node);
bool raylib_lite_ui_pointer(raylib_lite_ui_t *ui, int32_t track_id,
                            float x, float y, bool pressed);
bool raylib_lite_ui_action(raylib_lite_ui_t *ui, int direction, bool confirm);
void raylib_lite_ui_draw(raylib_lite_ui_t *ui, int font_size);

#ifdef __cplusplus
}
#endif
