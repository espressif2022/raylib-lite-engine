// SPDX-License-Identifier: Apache-2.0
#include "pet_draw.h"

#include <math.h>

#include "vg_raster.h"

/* Sticker palette: one warm outline colour and a handful of flat fills. */
#define C_OUT vg_rgb(62, 43, 44)
#define C_FUR vg_rgb(246, 168, 92)
#define C_FUR_DARK vg_rgb(222, 128, 64)
#define C_CREAM vg_rgb(255, 242, 224)
#define C_PINK vg_rgb(244, 140, 150)
#define C_WHITE vg_rgb(255, 255, 255)
#define C_BG vg_rgb(253, 236, 222)
#define C_BG_DISC vg_rgb(250, 221, 203)
#define C_BG_DOT vg_rgb(247, 212, 193)
#define C_RUG vg_rgb(242, 200, 180)
#define C_SHADOW vg_rgb(226, 176, 158)
#define C_HEART vg_rgb(240, 86, 104)
#define C_FISH vg_rgb(104, 160, 222)
#define C_BOLT vg_rgb(250, 190, 64)
#define C_YARN vg_rgb(172, 150, 228)
#define C_YARN_DARK vg_rgb(126, 104, 200)
#define C_MINT vg_rgb(126, 204, 186)
#define C_MINT_LIGHT vg_rgb(178, 228, 214)
#define C_KIBBLE vg_rgb(176, 112, 70)

#define OUTLINE 4.0f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void circle(float x, float y, float r, uint16_t color)
{
    vg_path_begin();
    vg_ellipse(x, y, r, r);
    vg_fill(color, 255);
}

static void ellipse(float x, float y, float rx, float ry, uint16_t color, uint8_t alpha)
{
    vg_path_begin();
    vg_ellipse(x, y, rx, ry);
    vg_fill(color, alpha);
}

static void heart_path(float x, float y, float s)
{
    vg_path_begin();
    vg_move_to(x, y + s * 0.9f);
    vg_cubic_to(x - s * 1.2f, y + s * 0.1f, x - s * 0.95f, y - s * 0.95f, x, y - s * 0.35f);
    vg_cubic_to(x + s * 0.95f, y - s * 0.95f, x + s * 1.2f, y + s * 0.1f, x, y + s * 0.9f);
    vg_close();
}

/* ---------------------------------------------------------------- room */

void pet_draw_background(uint16_t *pixels, int width, int height, size_t stride)
{
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) pixels[(size_t)y * stride + x] = C_BG;
    vg_begin(pixels, width, height, stride);
    vg_set_matrix(vg_identity());
    circle(240, 236, 196, C_BG_DISC);
    static const float dots[][3] = {
        {46, 96, 4}, {92, 150, 3}, {60, 230, 5}, {420, 110, 5}, {446, 196, 3},
        {392, 250, 4}, {30, 320, 3}, {452, 318, 4}, {130, 70, 3}, {356, 64, 3},
    };
    for (size_t i = 0; i < sizeof(dots) / sizeof(dots[0]); ++i)
        circle(dots[i][0], dots[i][1], dots[i][2], C_BG_DOT);
    ellipse(240, 382, 178, 30, C_RUG, 255);
    ellipse(240, 382, 150, 22, C_BG_DOT, 255);
    ellipse(240, 376, 104, 12, C_SHADOW, 255);
}

/* ----------------------------------------------------------------- cat */

static void body_path(float g)
{
    vg_path_begin();
    vg_move_to(0, -150 - g);
    vg_cubic_to(52 + g, -150 - g, 80 + g, -100, 82 + g, -40);
    vg_cubic_to(84 + g, -8 + g, 60, g, 0, g);
    vg_cubic_to(-60, g, -84 - g, -8 + g, -82 - g, -40);
    vg_cubic_to(-80 - g, -100, -52 - g, -150 - g, 0, -150 - g);
    vg_close();
}

static void head_path(float g)
{
    const float rx = 106 + g, top = -84 - g, bot = 78 + g;
    vg_path_begin();
    vg_move_to(0, top);
    vg_cubic_to(rx * 0.64f, top, rx, top * 0.55f, rx, 0);
    vg_line_to(rx + 12 + g * 0.4f, 20 + g * 0.4f);
    vg_line_to(rx - 3, 28);
    vg_cubic_to(rx * 0.8f, bot, rx * 0.45f, bot, 0, bot);
    vg_cubic_to(-rx * 0.45f, bot, -rx * 0.8f, bot, -(rx - 3), 28);
    vg_line_to(-(rx + 12 + g * 0.4f), 20 + g * 0.4f);
    vg_line_to(-rx, 0);
    vg_cubic_to(-rx, top * 0.55f, -rx * 0.64f, top, 0, top);
    vg_close();
}

static void ear_path(float g)
{
    vg_path_begin();
    vg_move_to(-34 - g, 18 + g);
    vg_line_to(8 - g * 0.6f, -46 - g * 0.8f);
    vg_quad_to(18, -60 - g * 1.6f, 27 + g * 0.6f, -44 - g * 0.8f);
    vg_line_to(38 + g, 18 + g);
    vg_close();
}

static void draw_ear(vg_mat_t head, float s, float angle, float shift)
{
    vg_mat_t m = vg_translate(head, s * 64 + shift, -66);
    m = vg_scale(m, s, 1);
    vg_set_matrix(vg_rotate(m, angle));
    ear_path(OUTLINE);
    vg_fill(C_OUT, 255);
    ear_path(0);
    vg_fill(C_FUR, 255);
    vg_path_begin();
    vg_move_to(-16, 12);
    vg_line_to(10, -28);
    vg_quad_to(16, -38, 21, -28);
    vg_line_to(26, 12);
    vg_close();
    vg_fill(C_PINK, 255);
}

static void rotate_about(float bx, float by, float x, float y, float a, float *ox, float *oy)
{
    float c = cosf(a), s = sinf(a), dx = x - bx, dy = y - by;
    *ox = bx + dx * c - dy * s;
    *oy = by + dx * s + dy * c;
}

static void draw_tail(float angle)
{
    const float bx = 56, by = -30;
    float x1, y1, x2, y2, x3, y3;
    rotate_about(bx, by, 118, -24, angle * 0.4f, &x1, &y1);
    rotate_about(bx, by, 134, -104, angle * 0.8f, &x2, &y2);
    rotate_about(bx, by, 98, -140, angle, &x3, &y3);
    vg_stroke_cubic(bx, by, x1, y1, x2, y2, x3, y3, 26 + OUTLINE * 2, 18 + OUTLINE * 2,
                    C_OUT, 255);
    circle(x3, y3, 9 + OUTLINE, C_OUT);
    vg_stroke_cubic(bx, by, x1, y1, x2, y2, x3, y3, 26, 18, C_FUR, 255);
    circle(x3, y3, 9, C_FUR_DARK);
}

static void draw_paw(float s, float raise)
{
    float x = s * (30 + 34 * raise), y = -8 - 80 * raise;
    ellipse(x, y, 21 + OUTLINE, 15 + OUTLINE, C_OUT, 255);
    ellipse(x, y, 21, 15, C_CREAM, 255);
    vg_stroke_line(x - 6, y + 2, x - 6, y + 11, 2.5f, C_OUT, 255);
    vg_stroke_line(x + 6, y + 2, x + 6, y + 11, 2.5f, C_OUT, 255);
}

static void draw_body(const pet_pose_t *pose)
{
    body_path(OUTLINE);
    vg_fill(C_OUT, 255);
    body_path(0);
    vg_fill(C_FUR, 255);
    ellipse(0, -58, 42, 50, C_CREAM, 255);
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        vg_stroke_cubic(s * 46, -4, s * 74, -14, s * 80, -56, s * 60, -84, 3.5f, 1.5f,
                        C_OUT, 255);
    }
    draw_paw(-1, pose->paw[0]);
    draw_paw(1, pose->paw[1]);
}

static void draw_eye(float ex, float ey, const pet_pose_t *pose)
{
    float open = pose->eye_open;
    if (pose->eye_happy > 0.5f) {
        vg_stroke_cubic(ex - 15, ey + 5, ex - 8, ey - 12, ex + 8, ey - 12, ex + 15, ey + 5,
                        5.5f, 5.5f, C_OUT, 255);
    } else if (open < 0.18f) {
        vg_stroke_cubic(ex - 15, ey, ex - 7, ey + 9, ex + 7, ey + 9, ex + 15, ey, 5, 5,
                        C_OUT, 255);
    } else {
        float ry = 20 * open, rx = 16 * (open > 1 ? open : 1);
        ellipse(ex, ey, rx, ry, C_OUT, 255);
        if (open > 0.45f) {
            circle(ex + 5 - pose->look_x * 2, ey - 7 * open, 6, C_WHITE);
            circle(ex - 5, ey + 8 * open, 2.6f, C_WHITE);
        }
    }
}

static void draw_face(const pet_pose_t *pose, float time)
{
    float fx = pose->look_x * 7, fy = pose->look_y * 5;
    for (int i = -1; i <= 1; ++i) {
        float x = (float)i * 20;
        vg_stroke_line(x, -80, x * 1.1f, -58, 6, C_FUR_DARK, 255);
    }
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        vg_stroke_line(s * 100, -10, s * 80, -6, 5, C_FUR_DARK, 255);
        vg_stroke_line(s * 102, 4, s * 84, 6, 4, C_FUR_DARK, 255);
    }
    ellipse(-13 + fx * 1.1f, 40 + fy, 21, 15, C_CREAM, 255);
    ellipse(13 + fx * 1.1f, 40 + fy, 21, 15, C_CREAM, 255);
    uint8_t blush = (uint8_t)clampf(90 + 130 * pose->blush, 0, 255);
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        ellipse(s * 68 + fx, 32 + fy, 15, 8, C_PINK, blush);
        draw_eye(s * 42 + fx, 4 + fy, pose);
        if (pose->sad > 0.3f)
            vg_stroke_line(s * 42 + fx - s * 12, fy - 32, s * 42 + fx + s * 12, fy - 24,
                           4, C_OUT, (uint8_t)clampf(pose->sad * 255, 0, 255));
    }

    float nx = fx * 1.2f, ny = 28 + fy;
    float m = pose->mouth_open;
    if (m > 0.08f) {
        float my = ny + 14 + 5 * m;
        ellipse(nx, my, 10 + 3 * m, 4 + 13 * m, C_OUT, 255);
        ellipse(nx, my + 3 + 6 * m, 7 + 2 * m, 2 + 5 * m, C_PINK, 255);
    } else {
        vg_stroke_cubic(nx, ny + 4, nx, ny + 12, nx - 9, ny + 14, nx - 14, ny + 8, 3.5f, 3,
                        C_OUT, 255);
        vg_stroke_cubic(nx, ny + 4, nx, ny + 12, nx + 9, ny + 14, nx + 14, ny + 8, 3.5f, 3,
                        C_OUT, 255);
    }
    vg_path_begin();
    vg_move_to(nx - 9, ny - 5);
    vg_quad_to(nx, ny - 8, nx + 9, ny - 5);
    vg_quad_to(nx + 4, ny + 4, nx, ny + 5);
    vg_quad_to(nx - 4, ny + 4, nx - 9, ny - 5);
    vg_fill(C_PINK, 255);

    float twitch = 1.5f * sinf(time * 3.1f);
    for (int i = 0; i < 2; ++i) {
        float s = i ? 1.0f : -1.0f;
        vg_stroke_line(s * 84 + fx, 30 + fy, s * 126 + fx, 22 + fy + twitch, 2.4f, C_OUT, 255);
        vg_stroke_line(s * 84 + fx, 40 + fy, s * 124 + fx, 44 + fy - twitch, 2.4f, C_OUT, 255);
    }
}

static void draw_cat(const pet_t *pet)
{
    const pet_pose_t *pose = &pet->pose;
    vg_mat_t cat = vg_translate(vg_identity(), PET_X, PET_FLOOR_Y);
    vg_mat_t body = vg_scale(cat, 1 - pose->breath * 0.01f, 1 + pose->breath * 0.018f);
    vg_set_matrix(body);
    draw_tail(pose->tail);
    draw_body(pose);

    float jitter = pose->purr * 0.8f * sinf(pet->time * 70);
    vg_mat_t head = vg_translate(cat, pose->head_x + jitter, -PET_HEAD_RISE + pose->head_y);
    head = vg_rotate(head, pose->head_tilt);
    draw_ear(head, -1, pose->ear_l, -pose->look_x * 3);
    draw_ear(head, 1, pose->ear_r, -pose->look_x * 3);
    vg_set_matrix(head);
    head_path(OUTLINE);
    vg_fill(C_OUT, 255);
    head_path(0);
    vg_fill(C_FUR, 255);
    draw_face(pose, pet->time);
}

/* --------------------------------------------------------------- props */

static void bowl_path(float g)
{
    vg_path_begin();
    vg_move_to(-64 - g, -g);
    vg_line_to(64 + g, -g);
    vg_cubic_to(62 + g, 26 + g, 50, 34 + g, 0, 34 + g);
    vg_cubic_to(-50, 34 + g, -62 - g, 26 + g, -64 - g, -g);
    vg_close();
}

static void draw_bowl(const pet_t *pet)
{
    static const float kibble[PET_KIBBLE][2] = {
        {-30, -2}, {30, -2}, {-12, -4}, {14, -4}, {0, -12}, {-2, 2},
    };
    vg_set_matrix(vg_translate(vg_identity(), PET_X, 366));
    for (int i = 0; i < pet->kibble; ++i) {
        ellipse(kibble[i][0], kibble[i][1], 11 + OUTLINE * 0.6f, 8 + OUTLINE * 0.6f, C_OUT, 255);
        ellipse(kibble[i][0], kibble[i][1], 11, 8, C_KIBBLE, 255);
    }
    bowl_path(OUTLINE);
    vg_fill(C_OUT, 255);
    bowl_path(0);
    vg_fill(C_MINT, 255);
    vg_path_begin();
    vg_round_rect(-60, 2, 120, 7, 3.5f);
    vg_fill(C_MINT_LIGHT, 255);
    heart_path(0, 20, 7);
    vg_fill(C_WHITE, 255);
    vg_set_matrix(vg_identity());
}

static void draw_ball(const pet_t *pet)
{
    float lift = clampf((PET_BALL_FLOOR - pet->ball_y) / 200, 0, 1);
    vg_set_matrix(vg_identity());
    ellipse(pet->ball_x, PET_BALL_FLOOR + PET_BALL_R, PET_BALL_R * (1 - 0.5f * lift), 5,
            C_SHADOW, 200);
    vg_mat_t m = vg_rotate(vg_translate(vg_identity(), pet->ball_x, pet->ball_y), pet->ball_spin);
    vg_set_matrix(m);
    vg_stroke_cubic(12, 14, 24, 26, 34, 18, 40, 26, 3, 3, C_OUT, 255);
    circle(0, 0, PET_BALL_R + OUTLINE * 0.8f, C_OUT);
    circle(0, 0, PET_BALL_R, C_YARN);
    vg_stroke_cubic(-16, -8, -6, -16, 8, -16, 17, -6, 3, 3, C_YARN_DARK, 255);
    vg_stroke_cubic(-18, 2, -6, -6, 8, -6, 18, 4, 3, 3, C_YARN_DARK, 255);
    vg_stroke_cubic(-14, 12, -4, 4, 8, 4, 14, 12, 3, 3, C_YARN_DARK, 255);
    vg_set_matrix(vg_identity());
}

static void draw_z(float x, float y, float s, uint8_t alpha)
{
    vg_stroke_line(x - s, y - s, x + s, y - s, s * 0.45f, C_OUT, alpha);
    vg_stroke_line(x + s, y - s, x - s, y + s, s * 0.45f, C_OUT, alpha);
    vg_stroke_line(x - s, y + s, x + s, y + s, s * 0.45f, C_OUT, alpha);
}

static void draw_fx(const pet_t *pet)
{
    vg_set_matrix(vg_identity());
    for (int i = 0; i < PET_FX_COUNT; ++i) {
        const pet_fx_t *fx = &pet->fx[i];
        if (!fx->live) continue;
        float u = fx->age / fx->life;
        uint8_t alpha = (uint8_t)clampf((1 - u) * 3 * 255, 0, 255);
        if (fx->kind == PET_FX_HEART) {
            float pop = u < 0.15f ? u / 0.15f : 1;
            heart_path(fx->x, fx->y, fx->size * pop + 2.5f);
            vg_fill(C_OUT, alpha);
            heart_path(fx->x, fx->y, fx->size * pop);
            vg_fill(C_HEART, alpha);
        } else {
            draw_z(fx->x, fx->y, fx->size * 0.6f, alpha);
        }
    }
}

void pet_draw_scene(const pet_t *pet)
{
    draw_cat(pet);
    if (pet->mode == PET_MODE_EAT) draw_bowl(pet);
    if (pet->mode == PET_MODE_PLAY) draw_ball(pet);
    draw_fx(pet);
    vg_set_matrix(vg_identity());
}

/* ------------------------------------------------------------------ UI */

static void fish_icon(float x, float y, float s, uint16_t color)
{
    vg_path_begin();
    vg_ellipse(x - 2 * s, y, 9 * s, 6 * s);
    vg_fill(color, 255);
    vg_path_begin();
    vg_move_to(x + 5 * s, y);
    vg_line_to(x + 12 * s, y - 6 * s);
    vg_line_to(x + 12 * s, y + 6 * s);
    vg_close();
    vg_fill(color, 255);
}

static void bolt_icon(float x, float y, float s, uint16_t color)
{
    vg_path_begin();
    vg_move_to(x + 2 * s, y - 9 * s);
    vg_line_to(x - 6 * s, y + 1 * s);
    vg_line_to(x - 0.5f * s, y + 1 * s);
    vg_line_to(x - 2 * s, y + 9 * s);
    vg_line_to(x + 6 * s, y - 1.5f * s);
    vg_line_to(x + 0.5f * s, y - 1.5f * s);
    vg_close();
    vg_fill(color, 255);
}

static void round_rect(float x, float y, float w, float h, float r, uint16_t color)
{
    vg_path_begin();
    vg_round_rect(x, y, w, h, r);
    vg_fill(color, 255);
}

static void draw_stat(float x0, int icon, float value, uint16_t color)
{
    round_rect(x0 - 2.5f, 15.5f, 137, 39, 19.5f, C_OUT);
    round_rect(x0, 18, 132, 34, 17, C_WHITE);
    circle(x0 + 18, 35, 12, color);
    if (icon == 0) {
        heart_path(x0 + 18, 35, 6.5f);
        vg_fill(C_WHITE, 255);
    } else if (icon == 1) {
        fish_icon(x0 + 17, 35, 0.8f, C_WHITE);
    } else {
        bolt_icon(x0 + 18, 35, 0.9f, C_WHITE);
    }
    round_rect(x0 + 38, 30, 82, 10, 5, C_BG_DOT);
    float w = 82 * clampf(value, 0, 100) / 100;
    if (w > 10) round_rect(x0 + 38, 30, w, 10, 5, color);
}

static float button_x(int i) { return 150.0f + 90.0f * (float)i; }

int pet_button_at(int x, int y)
{
    for (int i = 0; i < PET_BUTTON_COUNT; ++i) {
        float dx = (float)x - button_x(i), dy = (float)(y - PET_BUTTON_Y);
        if (dx * dx + dy * dy <= (PET_BUTTON_R + 6) * (PET_BUTTON_R + 6))
            return PET_MODE_EAT + i;
    }
    return -1;
}

void pet_draw_ui(const pet_t *pet)
{
    vg_set_matrix(vg_identity());
    draw_stat(30, 0, pet->love, C_HEART);
    draw_stat(174, 1, pet->food, C_FISH);
    draw_stat(318, 2, pet->energy, C_BOLT);

    const uint16_t active_colors[] = {
        vg_rgb(255, 214, 160), vg_rgb(214, 202, 246), vg_rgb(190, 206, 244),
    };
    for (int i = 0; i < PET_BUTTON_COUNT; ++i) {
        float x = button_x(i), y = PET_BUTTON_Y;
        bool active = pet->mode == (pet_mode_t)(PET_MODE_EAT + i);
        float press = active ? 2 : 0;
        circle(x, y + 3, PET_BUTTON_R + 3, C_SHADOW);
        circle(x, y + press, PET_BUTTON_R + 3, C_OUT);
        circle(x, y + press, PET_BUTTON_R, active ? active_colors[i] : C_WHITE);
        y += press;
        if (i == 0) {
            fish_icon(x - 1, y, 1.3f, C_FISH);
            circle(x - 10, y - 2, 2, C_WHITE);
        } else if (i == 1) {
            circle(x, y, 13, C_YARN);
            vg_stroke_cubic(x - 10, y - 5, x - 3, y - 10, x + 4, y - 10, x + 10, y - 4, 2.5f,
                            2.5f, C_YARN_DARK, 255);
            vg_stroke_cubic(x - 12, y + 3, x - 4, y - 3, x + 5, y - 3, x + 12, y + 3, 2.5f,
                            2.5f, C_YARN_DARK, 255);
        } else {
            circle(x, y, 13, C_BOLT);
            circle(x + 7, y - 5, 11, active ? active_colors[i] : C_WHITE);
        }
    }
}
