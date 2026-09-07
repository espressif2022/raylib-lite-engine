// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MOSAICO_UI_CAPACITY 32
#define MOSAICO_UI_POINTER_CAPACITY 2
typedef enum { MOSAICO_UI_PANEL,MOSAICO_UI_LABEL,MOSAICO_UI_BUTTON } mosaico_ui_type_t;
typedef void (*mosaico_ui_click_cb_t)(uint16_t id,void *context);
typedef struct { uint16_t id;mosaico_ui_type_t type;Rectangle bounds;const char *text;Color color,text_color;bool visible,pressed;mosaico_ui_click_cb_t click;void *context; } mosaico_ui_node_t;
typedef struct { int32_t track_id;float x,y;bool active; } mosaico_ui_pointer_t;
typedef struct { mosaico_ui_node_t nodes[MOSAICO_UI_CAPACITY];size_t count;int focused;mosaico_ui_pointer_t pointers[MOSAICO_UI_POINTER_CAPACITY]; } mosaico_ui_t;
void mosaico_ui_init(mosaico_ui_t *ui);
bool mosaico_ui_add(mosaico_ui_t *ui,mosaico_ui_node_t node);
bool mosaico_ui_pointer(mosaico_ui_t *ui,int32_t track_id,float x,float y,bool pressed);
bool mosaico_ui_action(mosaico_ui_t *ui,int direction,bool confirm);
void mosaico_ui_draw(mosaico_ui_t *ui,int font_size);
#ifdef __cplusplus
}
#endif
