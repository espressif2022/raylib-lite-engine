// SPDX-License-Identifier: Apache-2.0
#include "cat_draw.h"

#include <math.h>

#include "vg_raster.h"

/* Grey-brown tabby, lit from above: stripe < shade < base < hi < light. */
#define C_DARK vg_rgb(62, 55, 50)
#define C_STRIPE vg_rgb(88, 78, 70)
#define C_SHADE vg_rgb(112, 102, 92)
#define C_BASE vg_rgb(146, 134, 120)
#define C_HI vg_rgb(168, 157, 143)
#define C_LIGHT vg_rgb(210, 202, 190)
#define C_EAR_IN vg_rgb(190, 150, 140)
#define C_NOSE vg_rgb(190, 126, 118)
#define C_IRIS vg_rgb(192, 178, 70)
#define C_IRIS_RIM vg_rgb(150, 140, 60)
#define C_PUPIL vg_rgb(24, 20, 18)
#define C_WHISKER vg_rgb(240, 236, 228)
#define C_MOUTH vg_rgb(78, 44, 44)
#define C_TONGUE vg_rgb(214, 128, 128)

#define C_WALL vg_rgb(226, 218, 204)
#define C_WALL_LIT vg_rgb(236, 230, 218)
#define C_TRIM vg_rgb(204, 192, 176)
#define C_FLOOR vg_rgb(184, 156, 126)
#define C_FLOOR_LINE vg_rgb(168, 140, 112)
#define C_FLOOR_LIT vg_rgb(198, 172, 142)
#define C_FLOOR_SHADOW vg_rgb(150, 124, 100)

#define FLOOR_TOP 336.0f

static void fill_ellipse(float x, float y, float rx, float ry, uint16_t color)
{
    vg_path_begin();
    vg_ellipse(x, y, rx, ry);
    vg_fill(color, 255);
}

/* Quad plus round ends, wound like vg_ellipse so unions have no holes. */
static void add_capsule(float x0, float y0, float w0, float x1, float y1, float w1)
{
    float tx = x1 - x0, ty = y1 - y0, len = sqrtf(tx * tx + ty * ty);
    if (len > 1e-3f) {
        float nx = -ty / len, ny = tx / len, h0 = w0 * 0.5f, h1 = w1 * 0.5f;
        vg_move_to(x0 - nx * h0, y0 - ny * h0);
        vg_line_to(x1 - nx * h1, y1 - ny * h1);
        vg_line_to(x1 + nx * h1, y1 + ny * h1);
        vg_line_to(x0 + nx * h0, y0 + ny * h0);
        vg_close();
    }
    vg_ellipse(x0, y0, w0 * 0.5f, w0 * 0.5f);
    vg_ellipse(x1, y1, w1 * 0.5f, w1 * 0.5f);
}

static void fill_capsule(float x0, float y0, float w0, float x1, float y1, float w1,
                         uint16_t color)
{
    vg_path_begin();
    add_capsule(x0, y0, w0, x1, y1, w1);
    vg_fill(color, 255);
}

/* ---------------------------------------------------------------- room */

void cat_draw_background(uint16_t *pixels, int width, int height, size_t stride)
{
    for (int y = 0; y < height; ++y) {
        uint16_t c = y < (int)FLOOR_TOP ? C_WALL : C_FLOOR;
        for (int x = 0; x < width; ++x) pixels[(size_t)y * stride + x] = c;
    }
    vg_begin(pixels, width, height, stride);
    vg_set_matrix(vg_identity());
    vg_path_begin();
    vg_round_rect(330, 50, 120, 170, 4);
    vg_fill(C_WALL_LIT, 255);
    vg_path_begin();
    vg_round_rect(330, 130, 120, 6, 2);
    vg_fill(C_TRIM, 255);
    vg_path_begin();
    vg_round_rect(387, 50, 6, 170, 2);
    vg_fill(C_TRIM, 255);
    vg_path_begin();
    vg_move_to(290, FLOOR_TOP + 4);
    vg_line_to(440, FLOOR_TOP + 4);
    vg_line_to(480, 420);
    vg_line_to(300, 480);
    vg_line_to(250, 480);
    vg_close();
    vg_fill(C_FLOOR_LIT, 255);
    vg_path_begin();
    vg_round_rect(0, FLOOR_TOP - 16, (float)width, 16, 0);
    vg_fill(C_TRIM, 255);
    static const float boards[] = {364, 400, 446};
    for (int i = 0; i < 3; ++i) {
        vg_path_begin();
        vg_round_rect(0, boards[i], (float)width, 2, 0);
        vg_fill(C_FLOOR_LINE, 255);
    }
}

/* ---------------------------------------------------------------- body */

/* Sitting pear seen from the front; y = 0 is the floor. */
static void body_path(float breath, float dy)
{
    float w = 1 + breath * 0.012f;
    vg_path_begin();
    vg_move_to(0, -196 + dy);
    for (int side = 1; side >= -1; side -= 2) {
        float s = (float)side * w;
        if (side < 0) vg_move_to(0, -196 + dy);
        vg_cubic_to(s * 40, -196 + dy, s * 60, -176 + dy, s * 62, -146 + dy);
        vg_cubic_to(s * 66, -104 + dy, s * 98, -76 + dy, s * 98, -38 + dy);
        vg_cubic_to(s * 98, -12 + dy, s * 84, 0 + dy, s * 60, 0 + dy);
        vg_line_to(0, 0 + dy);
        vg_close();
    }
}

static void draw_tail(float sway)
{
    /* Wraps from behind the right haunch around the front paws. */
    float tip_x = -64 + sway * 10, tip_y = -14 - fabsf(sway) * 10;
    vg_stroke_cubic(96, -20, 104, 6, 40, 4, 0, -2, 22, 18, C_SHADE, 255);
    vg_stroke_cubic(96, -23, 104, 2, 40, 0, 0, -5, 18, 15, C_BASE, 255);
    vg_stroke_cubic(0, -2, -30, -4, -50, -2, tip_x, tip_y, 18, 12, C_SHADE, 255);
    vg_stroke_cubic(0, -5, -30, -7, -50, -5, tip_x, tip_y - 2, 15, 10, C_BASE, 255);
    vg_stroke_line(62, -8, 64, 6, 4, C_STRIPE, 255);
    vg_stroke_line(28, -10, 26, 4, 4, C_STRIPE, 255);
    vg_stroke_line(-22, -12, -24, 1, 4, C_STRIPE, 255);
    fill_capsule(tip_x * 0.7f - 45 * 0.3f, tip_y * 0.7f - 4 * 0.3f, 11, tip_x, tip_y - 1, 9,
                 C_DARK);
}

static void draw_body(float breath, float tail)
{
    body_path(breath, 5);
    vg_fill(C_SHADE, 255);
    body_path(breath, 0);
    vg_fill(C_BASE, 255);

    fill_ellipse(0, -140, 30, 40, C_HI);
    fill_ellipse(0, -150, 18, 26, C_LIGHT);

    for (int side = -1; side <= 1; side += 2) {
        float s = (float)side;
        /* Haunch and its contour. */
        vg_stroke_cubic(s * 46, -92, s * 72, -86, s * 86, -56, s * 82, -24, 3, 1, C_SHADE, 255);
        vg_stroke_cubic(s * 60, -116, s * 74, -108, s * 86, -96, s * 94, -80, 5, 1, C_STRIPE, 255);
        vg_stroke_cubic(s * 66, -74, s * 78, -70, s * 90, -60, s * 96, -46, 5, 1, C_STRIPE, 255);
        vg_stroke_cubic(s * 56, -150, s * 60, -140, s * 62, -130, s * 64, -120, 4, 1, C_STRIPE, 255);
        fill_ellipse(s * 70, -6, 17, 8, C_SHADE);
        fill_ellipse(s * 70, -8, 15, 6.5f, C_HI);
    }
    draw_tail(tail);
    for (int side = -1; side <= 1; side += 2) {
        float s = (float)side;
        /* Front leg: shadowed outer edge, then the leg and a lit inner edge. */
        fill_capsule(s * 26, -130, 32, s * 26, -14, 24, C_SHADE);
        fill_capsule(s * 24, -132, 28, s * 24, -14, 22, C_BASE);
        vg_stroke_line(s * 16, -110, s * 17, -24, 3, C_HI, 255);
        vg_stroke_line(s * 33, -70, s * 14, -66, 4, C_STRIPE, 255);
        vg_stroke_line(s * 34, -46, s * 14, -42, 4, C_STRIPE, 255);
        fill_ellipse(s * 26, -8, 19, 10, C_SHADE);
        fill_ellipse(s * 26, -10, 18, 9, C_LIGHT);
        for (int t = -1; t <= 1; ++t)
            vg_stroke_line(s * 26 + (float)t * 6, -12, s * 26 + (float)t * 7, -3, 1.3f,
                           C_SHADE, 255);
    }
}

/* ---------------------------------------------------------------- head */

static void head_path(float dy)
{
    vg_path_begin();
    for (int side = 1; side >= -1; side -= 2) {
        float s = (float)side;
        vg_move_to(0, -62 + dy);
        vg_cubic_to(s * 28, -64 + dy, s * 50, -56 + dy, s * 60, -40 + dy);
        vg_cubic_to(s * 70, -26 + dy, s * 76, -8 + dy, s * 78, 6 + dy);
        vg_line_to(s * 90, 14 + dy);
        vg_line_to(s * 78, 20 + dy);
        vg_line_to(s * 86, 29 + dy);
        vg_line_to(s * 71, 33 + dy);
        vg_cubic_to(s * 58, 48 + dy, s * 30, 60 + dy, 0, 60 + dy);
        vg_close();
    }
}

static void draw_ear(vg_mat_t head, float s, float twitch, float shift)
{
    vg_mat_t m = vg_translate(head, s * 42 + shift, -50);
    m = vg_rotate(vg_scale(m, s, 1), -0.08f + twitch);
    vg_set_matrix(m);
    vg_path_begin();
    vg_move_to(-26, 4);
    vg_quad_to(-10, -40, 12, -58);
    vg_quad_to(26, -26, 24, 16);
    vg_close();
    vg_fill(C_BASE, 255);
    vg_path_begin();
    vg_move_to(-15, 0);
    vg_quad_to(-4, -30, 11, -44);
    vg_quad_to(18, -20, 15, 6);
    vg_close();
    vg_fill(C_EAR_IN, 255);
    for (int i = 0; i < 3; ++i) {
        float x = -8 + (float)i * 6;
        vg_stroke_cubic(x, 4, x + 1, -6, x + 3, -14, x + 6, -22, 1.3f, 0.5f, C_LIGHT, 255);
    }
    vg_set_matrix(head);
}

/* Outer corner up and out, inner corner low toward the nose; s picks the side. */
static void eye_path(float cx, float cy, float s, float rx, float ry, float grow)
{
    float ix = cx - s * (rx + grow), ox = cx + s * (rx + grow);
    vg_path_begin();
    vg_move_to(ix, cy + 3);
    vg_cubic_to(cx - s * rx * 0.5f, cy - ry - grow * 1.3f, cx + s * rx * 0.4f, cy - ry - grow,
                ox, cy - 3);
    vg_cubic_to(cx + s * rx * 0.5f, cy + ry + grow, cx - s * rx * 0.4f, cy + ry + grow,
                ix, cy + 3);
    vg_close();
}

static void draw_eye(float cx, float cy, float s, const cat_pose_t *pose)
{
    const float rx = 15, open = pose->eye_open;
    fill_ellipse(cx, cy + 1, rx + 7, 14, C_HI);
    if (open < 0.18f) {
        vg_stroke_cubic(cx - s * rx, cy + 3, cx - s * rx * 0.3f, cy + 7, cx + s * rx * 0.4f,
                        cy + 5, cx + s * rx, cy - 3, 2.6f, 1.6f, C_DARK, 255);
        return;
    }
    const float ry = 11 * open;
    eye_path(cx, cy, s, rx, ry, 2);
    vg_fill(C_DARK, 255);
    eye_path(cx, cy, s, rx, ry, 0);
    vg_fill(C_IRIS_RIM, 255);
    eye_path(cx, cy + 0.5f, s, rx - 2, ry - 1.5f, 0);
    vg_fill(C_IRIS, 255);
    float px = cx + pose->look_x * 4, py = cy + pose->look_y * 2;
    fill_ellipse(px, py, 2.4f + pose->pupil * 5, ry * 0.92f, C_PUPIL);
    if (open > 0.5f) {
        fill_ellipse(cx - 4, cy - ry * 0.4f, 2.6f, 2.6f, C_WHISKER);
        fill_ellipse(cx + 5, cy + ry * 0.4f, 1.2f, 1.2f, C_WHISKER);
    }
    /* Tear line from the inner corner toward the nose. */
    vg_stroke_line(cx - s * (rx + 1), cy + 3, cx - s * (rx - 2), cy + 12, 2, C_STRIPE, 255);
}

static void draw_head(const cat_pose_t *pose, vg_mat_t cat)
{
    vg_mat_t head = vg_translate(cat, pose->look_x * 8, CAT_HEAD_Y + 50 + pose->look_y * 5);
    head = vg_rotate(head, pose->tilt);
    head = vg_translate(head, 0, -50);
    const float fx = pose->look_x * 6, fy = pose->look_y * 4;

    draw_ear(head, -1, -pose->ear[0], -fx * 0.4f);
    draw_ear(head, 1, pose->ear[1], -fx * 0.4f);
    vg_set_matrix(head);
    head_path(3);
    vg_fill(C_SHADE, 255);
    head_path(0);
    vg_fill(C_BASE, 255);

    /* Forehead M, mascara lines and cheek bars. */
    vg_stroke_line(fx * 0.5f, -60, fx * 0.7f, -34, 4, C_STRIPE, 255);
    for (int side = -1; side <= 1; side += 2) {
        float s = (float)side;
        vg_stroke_line(s * 11 + fx * 0.5f, -58, s * 7 + fx * 0.7f, -36, 3.6f, C_STRIPE, 255);
        vg_stroke_line(s * 24 + fx * 0.4f, -54, s * 17 + fx * 0.6f, -38, 3.2f, C_STRIPE, 255);
        vg_stroke_cubic(s * 44 + fx, -6 + fy, s * 52, -4, s * 60, -2, s * 70, 2, 3.4f, 0.8f,
                        C_STRIPE, 255);
        vg_stroke_cubic(s * 48 + fx, 12, s * 56, 13, s * 64, 15, s * 74, 18, 3.2f, 0.8f,
                        C_STRIPE, 255);
        vg_stroke_cubic(s * 46 + fx, 22, s * 54, 24, s * 60, 27, s * 68, 30, 2.8f, 0.8f,
                        C_STRIPE, 255);
    }

    /* Muzzle: whisker pads, chin, nose and mouth. */
    float mx = fx * 1.1f, my = fy;
    vg_path_begin();
    vg_ellipse(mx - 13, 27 + my, 16, 11);
    vg_ellipse(mx + 13, 27 + my, 16, 11);
    vg_ellipse(mx, 42 + my, 12, 8);
    vg_fill(C_LIGHT, 255);
    fill_ellipse(mx, 6 + my, 8, 14, C_HI);

    for (int side = -1; side <= 1; side += 2) {
        float s = (float)side;
        draw_eye(s * 29 + fx, -8 + fy, s, pose);
    }

    float m = pose->mouth;
    if (m > 0.05f) {
        float oy = 34 + my, h = 4 + 18 * m;
        fill_ellipse(mx, oy + h * 0.5f, 10 + 6 * m, h, C_MOUTH);
        fill_ellipse(mx, oy + h * 1.1f, 7 + 4 * m, h * 0.45f, C_TONGUE);
        for (int side = -1; side <= 1; side += 2) {
            float s = (float)side;
            vg_path_begin();
            vg_move_to(mx + s * (5 + 5 * m), oy + 1);
            vg_line_to(mx + s * (8 + 5 * m), oy + 1);
            vg_line_to(mx + s * (6.5f + 5 * m), oy + 4 + 5 * m);
            vg_close();
            vg_fill(C_WHISKER, 255);
        }
    }
    vg_path_begin();
    vg_move_to(mx - 8, 13 + my);
    vg_quad_to(mx, 10 + my, mx + 8, 13 + my);
    vg_quad_to(mx + 4, 20 + my, mx, 22 + my);
    vg_quad_to(mx - 4, 20 + my, mx - 8, 13 + my);
    vg_fill(C_NOSE, 255);
    if (m <= 0.05f) {
        vg_stroke_line(mx, 22 + my, mx, 28 + my, 1.5f, C_DARK, 255);
        for (int side = -1; side <= 1; side += 2) {
            float s = (float)side;
            vg_stroke_cubic(mx, 28 + my, mx + s * 2, 32 + my, mx + s * 8, 33 + my,
                            mx + s * 12, 30 + my, 1.5f, 1, C_DARK, 255);
        }
    }

    for (int side = -1; side <= 1; side += 2) {
        float s = (float)side;
        for (int i = 0; i < 3; ++i) {
            float wy = 24 + (float)i * 4 + my;
            float ex = s * (100 - (float)i * 4) + fx * 0.6f;
            float ey = wy - 10 + (float)i * 9;
            vg_stroke_cubic(mx + s * 20, wy, mx + s * 44, wy - 4, ex - s * 24, ey - 3, ex, ey,
                            1.4f, 0.5f, C_WHISKER, 230);
        }
    }
}

void cat_draw(const cat_pose_t *pose)
{
    vg_set_matrix(vg_identity());
    fill_ellipse(CAT_X, CAT_GROUND_Y - 1, 116, 11, C_FLOOR_LINE);
    vg_path_begin();
    vg_ellipse(CAT_X, CAT_GROUND_Y - 1, 88, 5);
    vg_fill(C_FLOOR_SHADOW, 230);

    vg_mat_t cat = vg_translate(vg_identity(), CAT_X, CAT_GROUND_Y);
    vg_set_matrix(cat);
    draw_body(pose->breath, pose->tail);
    draw_head(pose, cat);
    vg_set_matrix(vg_identity());
}

bool cat_hit_head(const cat_pose_t *pose, float x, float y)
{
    float dx = (x - CAT_X - pose->look_x * 8) / 95;
    float dy = (y - CAT_GROUND_Y - CAT_HEAD_Y) / 90;
    return dx * dx + dy * dy <= 1;
}

/* -------------------------------------------------------------- buttons */

float cat_button_x(int index) { return 240.0f + (float)(index - 1) * (CAT_BUTTON_W + 12); }

int cat_button_at(int x, int y)
{
    for (int i = 0; i < CAT_ACT_COUNT; ++i) {
        float cx = cat_button_x(i);
        if ((float)x >= cx - CAT_BUTTON_W * 0.5f && (float)x <= cx + CAT_BUTTON_W * 0.5f &&
                y >= CAT_BUTTON_Y - CAT_BUTTON_H / 2 - 4 && y <= CAT_BUTTON_Y + CAT_BUTTON_H / 2 + 4)
            return i;
    }
    return -1;
}

void cat_draw_buttons(const cat_rig_t *rig)
{
    vg_set_matrix(vg_identity());
    for (int i = 0; i < CAT_ACT_COUNT; ++i) {
        float cx = cat_button_x(i);
        vg_path_begin();
        vg_round_rect(cx - CAT_BUTTON_W * 0.5f, CAT_BUTTON_Y - CAT_BUTTON_H * 0.5f,
                      CAT_BUTTON_W, CAT_BUTTON_H, CAT_BUTTON_H * 0.5f);
        vg_fill(cat_rig_playing(rig, (cat_action_t)i) ? C_SHADE : C_WALL_LIT, 255);
    }
}
