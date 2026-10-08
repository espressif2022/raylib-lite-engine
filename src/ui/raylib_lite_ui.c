// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_ui.h"
#include "raylib_lite_raylib_impl.h"

#include <string.h>

static bool hit(raylib_lite_renderer_rect_t rect, float x, float y)
{
    return x >= rect.x && y >= rect.y &&
           x < rect.x + rect.width && y < rect.y + rect.height;
}

static Color to_raylib_color(raylib_lite_renderer_color_t color)
{
    return (Color){color.r, color.g, color.b, color.a};
}

void raylib_lite_ui_init(raylib_lite_ui_t *ui)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->focused = -1;
}

bool raylib_lite_ui_add(raylib_lite_ui_t *ui, raylib_lite_ui_node_t node)
{
    if (!ui || ui->count == RAYLIB_LITE_UI_CAPACITY) return false;
    node.visible = true;
    ui->nodes[ui->count++] = node;
    return true;
}

bool raylib_lite_ui_pointer(raylib_lite_ui_t *ui, int32_t track_id,
                            float x, float y, bool down)
{
    if (!ui) return false;

    raylib_lite_ui_pointer_t *pointer = NULL;
    for (size_t i = 0; i < RAYLIB_LITE_UI_POINTER_CAPACITY; ++i) {
        if (ui->pointers[i].active && ui->pointers[i].track_id == track_id) {
            pointer = &ui->pointers[i];
            break;
        }
    }
    if (!pointer && down) {
        for (size_t i = 0; i < RAYLIB_LITE_UI_POINTER_CAPACITY; ++i) {
            if (!ui->pointers[i].active) {
                pointer = &ui->pointers[i];
                break;
            }
        }
    }

    bool used = false;
    if (pointer) {
        for (size_t i = 0; i < ui->count; ++i) {
            raylib_lite_ui_node_t *node = &ui->nodes[i];
            if (node->visible && node->type == RAYLIB_LITE_UI_BUTTON &&
                    hit(node->bounds, x, y)) {
                used = true;
                if (!down && hit(node->bounds, pointer->x, pointer->y) &&
                        node->click) {
                    node->click(node->id, node->context);
                }
            }
        }
        *pointer = (raylib_lite_ui_pointer_t) {
            .track_id = track_id,
            .x = x,
            .y = y,
            .active = down,
        };
    }

    for (size_t i = 0; i < ui->count; ++i) {
        raylib_lite_ui_node_t *node = &ui->nodes[i];
        if (node->type != RAYLIB_LITE_UI_BUTTON || !node->visible) continue;
        node->pressed = false;
        for (size_t p = 0; p < RAYLIB_LITE_UI_POINTER_CAPACITY; ++p) {
            if (ui->pointers[p].active &&
                    hit(node->bounds, ui->pointers[p].x, ui->pointers[p].y)) {
                node->pressed = true;
                break;
            }
        }
    }
    return used;
}

bool raylib_lite_ui_action(raylib_lite_ui_t *ui, int direction, bool confirm)
{
    if (!ui) return false;
    if (direction) {
        int focused = ui->focused;
        for (size_t i = 0; i < ui->count; ++i) {
            focused = (focused + direction + (int)ui->count) % (int)ui->count;
            if (ui->nodes[focused].visible &&
                    ui->nodes[focused].type == RAYLIB_LITE_UI_BUTTON) {
                ui->focused = focused;
                break;
            }
        }
    }
    if (confirm && ui->focused >= 0) {
        raylib_lite_ui_node_t *node = &ui->nodes[ui->focused];
        if (node->click) node->click(node->id, node->context);
        return true;
    }
    return ui->focused >= 0;
}

void raylib_lite_ui_draw(raylib_lite_ui_t *ui, int font_size)
{
    if (!ui) return;
    for (size_t i = 0; i < ui->count; ++i) {
        raylib_lite_ui_node_t *node = &ui->nodes[i];
        if (!node->visible) continue;

        Color color = to_raylib_color(node->color);
        if (node->pressed) {
            color = (Color){color.r / 2, color.g / 2, color.b / 2, color.a};
        }
        if (node->type != RAYLIB_LITE_UI_LABEL) {
            raylib_lite_raylib_draw_rectangle(
                (int)node->bounds.x, (int)node->bounds.y,
                (int)node->bounds.width, (int)node->bounds.height, color);
        }
        if (node->text) {
            int width = raylib_lite_raylib_measure_text(node->text, font_size);
            raylib_lite_raylib_draw_text(
                node->text,
                (int)(node->bounds.x + (node->bounds.width - width) / 2),
                (int)(node->bounds.y + (node->bounds.height - font_size) / 2),
                font_size, to_raylib_color(node->text_color));
        }
    }
}
