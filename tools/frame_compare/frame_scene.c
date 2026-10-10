// SPDX-License-Identifier: Apache-2.0
#include "frame_scene.h"

#include <math.h>

static const char *const SCENE_NAMES[] = { "primitives", "title", "raycast" };

int frame_scene_count(void) { return 3; }

const char *frame_scene_name(int index)
{
    if (index < 0 || index >= frame_scene_count()) return "unknown";
    return SCENE_NAMES[index];
}

void frame_scene_fill_texture(uint16_t *pixels, int width, int height)
{
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int row = y / 8;
            int brick_x = x + ((row & 1) ? 16 : 0);
            int mortar = (y % 8) == 0 || (brick_x % 32) == 0;
            unsigned r, g, b;
            if (mortar) { r = 40; g = 36; b = 32; }
            else if (((brick_x / 32) + row) & 1) { r = 170; g = 64; b = 42; }
            else { r = 96; g = 78; b = 58; }
            r = (r + (unsigned)(x & 7)) > 255 ? 255 : r + (unsigned)(x & 7);
            pixels[y * width + x] = (uint16_t)(((r & 0xf8u) << 8) | ((g & 0xfcu) << 3) | (b >> 3));
        }
    }
}

static int at(int design, int screen)
{
    int value = design * screen / 480;
    return value > 0 ? value : (design > 0 ? 1 : 0);
}

static void draw_primitives(int width, int height)
{
    ClearBackground((Color){ 18, 28, 42, 255 });
    DrawRectangle(at(16, width), at(16, height), at(140, width), at(90, height),
                  (Color){ 230, 41, 55, 255 });
    DrawRectangle(at(70, width), at(50, height), at(140, width), at(90, height),
                  (Color){ 0, 180, 90, 128 });
    DrawRectangleGradientV(at(230, width), at(16, height), at(120, width), at(200, height),
                           (Color){ 255, 220, 80, 255 }, (Color){ 40, 80, 180, 255 });
    DrawCircle(at(80, width), at(220, height), (float)at(48, width), (Color){ 255, 160, 40, 160 });
    DrawRectangleRounded((Rectangle){ (float)at(160, width), (float)at(200, height),
                                      (float)at(150, width), (float)at(80, height) },
                         0.4f, 8, (Color){ 80, 140, 255, 200 });
    DrawLineEx((Vector2){ (float)at(20, width), (float)at(340, height) },
               (Vector2){ (float)at(220, width), (float)at(420, height) },
               (float)at(8, width), (Color){ 255, 255, 255, 255 });
    DrawTriangle((Vector2){ (float)at(280, width), (float)at(360, height) },
                 (Vector2){ (float)at(430, width), (float)at(360, height) },
                 (Vector2){ (float)at(350, width), (float)at(250, height) },
                 (Color){ 200, 80, 220, 180 });
    DrawPoly((Vector2){ (float)at(400, width), (float)at(120, height) }, 6,
             (float)at(50, width), 15.0f, (Color){ 255, 255, 255, 220 });
    DrawText("Score 12", at(24, width), at(430, height), at(28, height),
             (Color){ 245, 245, 245, 255 });
}

static void draw_title(int width, int height, Texture2D texture)
{
    DrawRectangleGradientV(0, 0, width, height,
                           (Color){ 24, 36, 72, 255 }, (Color){ 180, 90, 40, 255 });
    DrawTexturePro(texture,
                   (Rectangle){ 0, 0, (float)texture.width, (float)texture.height },
                   (Rectangle){ (float)at(40, width), (float)at(70, height),
                                (float)at(400, width), (float)at(120, height) },
                   (Vector2){ 0, 0 }, 0, WHITE);
    for (int i = 0; i < 3; ++i) {
        DrawTexturePro(texture,
                       (Rectangle){ 0, 0, (float)texture.width, (float)texture.height },
                       (Rectangle){ (float)at(48 + i * 140, width), (float)at(250, height),
                                    (float)at(96, width), (float)at(96, height) },
                       (Vector2){ 0, 0 }, 0,
                       (Color){ 255, 255, 255, (unsigned char)(255 - i * 60) });
    }
    DrawTexturePro(texture,
                   (Rectangle){ 0, 0, (float)texture.width, (float)texture.height },
                   (Rectangle){ (float)at(300, width), (float)at(360, height),
                                (float)at(110, width), (float)at(110, height) },
                   (Vector2){ (float)at(55, width), (float)at(55, height) }, 20.0f, WHITE);
    DrawRectangle(at(150, width), at(400, height), at(180, width), at(48, height),
                  (Color){ 255, 203, 0, 230 });
    DrawText("TITLE", at(70, width), at(200, height), at(48, height),
             (Color){ 245, 245, 245, 255 });
}

static int map_blocked(int x, int y)
{
    static const char map[] =
        "########"
        "#......#"
        "#..##..#"
        "#......#"
        "#.#....#"
        "#....#.#"
        "#......#"
        "########";
    if (x < 0 || y < 0 || x >= 8 || y >= 8) return 1;
    return map[y * 8 + x] == '#';
}

static void draw_raycast(int width, int height, Texture2D texture)
{
    ClearBackground((Color){ 102, 191, 255, 255 });
    DrawRectangle(0, height / 2, width, height - height / 2, (Color){ 90, 70, 40, 255 });
    const float pos_x = 3.5f, pos_y = 4.5f;
    const float dir_x = 0.95f, dir_y = 0.31f;
    const float plane_x = -0.20f, plane_y = 0.62f;
    int tex_w = texture.width > 1 ? texture.width : FRAME_TEXTURE_SIZE;
    int tex_h = texture.height > 0 ? texture.height : FRAME_TEXTURE_SIZE;
    for (int x = 0; x < width; ++x) {
        float camera = 2.0f * (float)x / (float)width - 1.0f;
        float ray_x = dir_x + plane_x * camera;
        float ray_y = dir_y + plane_y * camera;
        int map_x = (int)pos_x, map_y = (int)pos_y;
        float delta_x = ray_x == 0.0f ? 1.0e30f : fabsf(1.0f / ray_x);
        float delta_y = ray_y == 0.0f ? 1.0e30f : fabsf(1.0f / ray_y);
        int step_x, step_y, side = 0, hit = 0;
        float side_x, side_y;
        if (ray_x < 0.0f) { step_x = -1; side_x = (pos_x - (float)map_x) * delta_x; }
        else { step_x = 1; side_x = ((float)map_x + 1.0f - pos_x) * delta_x; }
        if (ray_y < 0.0f) { step_y = -1; side_y = (pos_y - (float)map_y) * delta_y; }
        else { step_y = 1; side_y = ((float)map_y + 1.0f - pos_y) * delta_y; }
        for (int step = 0; step < 16 && !hit; ++step) {
            if (side_x < side_y) { side_x += delta_x; map_x += step_x; side = 0; }
            else { side_y += delta_y; map_y += step_y; side = 1; }
            if (map_blocked(map_x, map_y)) hit = 1;
        }
        if (!hit) continue;
        float perp = side == 0
            ? ((float)map_x - pos_x + (float)(1 - step_x) * 0.5f) / ray_x
            : ((float)map_y - pos_y + (float)(1 - step_y) * 0.5f) / ray_y;
        if (perp < 0.05f) perp = 0.05f;
        int line = (int)((float)height / perp);
        if (line < 1) line = 1;
        float wall = side == 0 ? pos_y + perp * ray_y : pos_x + perp * ray_x;
        wall -= floorf(wall);
        float tex_x = wall * (float)(tex_w - 1);
        Color tint = side ? (Color){ 170, 170, 170, 255 } : (Color){ 255, 255, 255, 255 };
        DrawTexturePro(texture, (Rectangle){ tex_x, 0, 1, (float)tex_h },
                       (Rectangle){ (float)x, (float)(height / 2 - line / 2), 1, (float)line },
                       (Vector2){ 0, 0 }, 0, tint);
    }
}

void frame_scene_draw(int scene, int width, int height, Texture2D texture)
{
    if (scene == 0) draw_primitives(width, height);
    else if (scene == 1) draw_title(width, height, texture);
    else draw_raycast(width, height, texture);
}
