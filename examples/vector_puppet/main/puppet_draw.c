// SPDX-License-Identifier: Apache-2.0
#include "puppet_draw.h"

#include <math.h>

#include "vg_raster.h"

#define C(hex) ((uint16_t)(((((hex) >> 16) & 0xf8U) << 8) | \
                            ((((hex) >> 8) & 0xfcU) << 3) | (((hex) & 0xffU) >> 3)))
#define DEG(d) ((d) * 0.017453293f)

enum {
    SKIN = C(0xffe6d8), SKIN_SHADE = C(0xf2c4b2), SKIN_LINE = C(0xd49c8c),
    HAIR = C(0x6b4a3a), HAIR_DARK = C(0x4e3429), HAIR_BACK = C(0x523628),
    HAIR_LIGHT = C(0x9a735c), LASH = C(0x2e211f), BROW = C(0x5a3e34),
    IRIS_OUT = C(0x2f3350), IRIS = C(0x4b5480), IRIS_GLOW = C(0x8a98cc),
    PUPIL = C(0x191b2c), SCLERA = C(0xfbfbff), WHITE = C(0xffffff),
    MOUTH = C(0x8c2f3e), TONGUE = C(0xe0707c), LIP = C(0x8a4a4a),
    BLUSH = C(0xff7f95), BLUSH_LINE = C(0xe85a75),
    RIBBON = C(0xc9303c), RIBBON_DARK = C(0x8e1c2a),
    CARDIGAN = C(0xe8c38c), CARDIGAN_SHADE = C(0xc79c64), CARDIGAN_RIB = C(0xdcb27a),
    BUTTON = C(0xa77a45), BUTTON_HI = C(0xf2dcb4),
    NAVY = C(0x262b47), NAVY_DARK = C(0x1c1f30), SKIRT = C(0x2a2e44),
    STRIPE = C(0xe6e9f2), SHIRT = C(0xf4f6fb), TIE = C(0x3552b8), TIE_DARK = C(0x243c8c),
    PETAL = C(0xffb3c6), PETAL_DARK = C(0xf08aa5),
};

static float s_yaw, s_pitch;
static vg_mat_t s_head;

static vg_mat_t head_layer(float depth)
{
    return vg_translate(s_head, s_yaw * depth, s_pitch * depth * 0.6f);
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ------------------------------------------------------------------ */
/* Background                                                         */
/* ------------------------------------------------------------------ */

static uint16_t lerp_rgb(uint32_t a, uint32_t b, float t)
{
    int r = (int)((a >> 16 & 255) + ((int)(b >> 16 & 255) - (int)(a >> 16 & 255)) * t);
    int g = (int)((a >> 8 & 255) + ((int)(b >> 8 & 255) - (int)(a >> 8 & 255)) * t);
    int bl = (int)((a & 255) + ((int)(b & 255) - (int)(a & 255)) * t);
    return vg_rgb((uint8_t)r, (uint8_t)g, (uint8_t)bl);
}

static uint32_t s_bg_seed;
static float bg_rand(void)
{
    s_bg_seed = s_bg_seed * 1103515245U + 12345U;
    return (float)((s_bg_seed >> 8) & 0xffff) / 65535.0f;
}

static void rect(float x, float y, float w, float h, uint16_t color, uint8_t alpha)
{
    vg_path_begin();
    vg_move_to(x, y);
    vg_line_to(x + w, y);
    vg_line_to(x + w, y + h);
    vg_line_to(x, y + h);
    vg_fill(color, alpha);
}

static void blossom(float x, float y, float r)
{
    for (int i = 0; i < 5; ++i) {
        float a = DEG(72.0f * i + 18);
        vg_path_begin();
        vg_ellipse(x + cosf(a) * r * 0.9f, y + sinf(a) * r * 0.9f, r * 0.75f, r * 0.75f);
        vg_fill(i & 1 ? C(0xffd6e2) : C(0xf7b6c8), 255);
    }
    vg_path_begin();
    vg_ellipse(x, y, r * 0.35f, r * 0.35f);
    vg_fill(C(0xe86f8e), 255);
}

void puppet_draw_background(uint16_t *px, int width, int height, size_t stride)
{
    s_bg_seed = 20260929U;
    const int wx0 = 52, wx1 = 428, wy0 = 28, wy1 = 300;
    for (int y = 0; y < height; ++y) {
        uint16_t *row = px + (size_t)y * stride;
        uint16_t wall = y < 360 ? lerp_rgb(0x3a3550, 0x2a2638, (float)y / 360.0f)
                                : lerp_rgb(0x4a3528, 0x2e2119, (float)(y - 360) / 120.0f);
        float t = (float)(y - wy0) / (float)(wy1 - wy0);
        uint16_t sky = t < 0.55f ? lerp_rgb(0x1d2a5c, 0x6a4f8a, t / 0.55f)
                                 : lerp_rgb(0x6a4f8a, 0xf0a070, (t - 0.55f) / 0.45f);
        for (int x = 0; x < width; ++x)
            row[x] = (y >= wy0 && y < wy1 && x >= wx0 && x < wx1) ? sky : wall;
    }

    vg_begin(px, width, height, stride);
    /* Stars and moon. */
    for (int i = 0; i < 40; ++i) {
        float x = wx0 + 8 + bg_rand() * (wx1 - wx0 - 16), y = wy0 + 6 + bg_rand() * 120;
        float r = 0.6f + bg_rand() * 1.0f;
        vg_path_begin();
        vg_ellipse(x, y, r, r);
        vg_fill(WHITE, (uint8_t)(120 + bg_rand() * 135));
    }
    vg_path_begin();
    vg_ellipse(356, 82, 36, 36);
    vg_fill(C(0xfff0c8), 36);
    vg_path_begin();
    vg_ellipse(356, 82, 22, 22);
    vg_fill(C(0xfff4d6), 255);
    vg_path_begin();
    vg_ellipse(364, 76, 18, 18);
    vg_fill(C(0xfbe6b4), 255);

    /* Hills and city skyline. */
    vg_path_begin();
    vg_move_to(wx0, 250);
    vg_cubic_to(120, 214, 180, 226, 240, 238);
    vg_cubic_to(300, 222, 380, 206, wx1, 240);
    vg_line_to(wx1, wy1);
    vg_line_to(wx0, wy1);
    vg_fill(C(0x3b2f55), 255);
    for (float x = wx0; x < wx1; ) {
        float w = 14 + bg_rand() * 26, h = 24 + bg_rand() * 62;
        if (x + w > wx1) w = wx1 - x;
        rect(x, wy1 - h, w, h, C(0x241f3a), 255);
        for (float wy = wy1 - h + 5; wy < wy1 - 6; wy += 8)
            for (float wxx = x + 3; wxx < x + w - 4; wxx += 6)
                if (bg_rand() < 0.35f) rect(wxx, wy, 2.5f, 3.5f, C(0xffd27a), 230);
        x += w + 2;
    }

    /* Sakura branch across the top-left pane. */
    vg_stroke_cubic(wx0, 60, 110, 70, 150, 40, 210, 52, 9, 3, C(0x4a3228), 255);
    vg_stroke_cubic(120, 62, 140, 90, 170, 96, 196, 118, 5, 1.5f, C(0x4a3228), 255);
    vg_stroke_cubic(wx0, 118, 84, 110, 96, 128, 120, 150, 6, 1.5f, C(0x4a3228), 255);
    static const float flowers[][3] = {
        {84, 58, 7}, {110, 66, 6}, {136, 50, 8}, {164, 44, 6}, {196, 52, 7},
        {150, 86, 6}, {176, 100, 7}, {198, 118, 5}, {72, 112, 6}, {98, 126, 7},
        {118, 148, 6}, {64, 76, 5}, {124, 38, 5}, {182, 66, 5}, {90, 94, 5},
    };
    for (unsigned i = 0; i < sizeof(flowers) / sizeof(flowers[0]); ++i)
        blossom(flowers[i][0], flowers[i][1], flowers[i][2]);

    /* Window frame. */
    const uint16_t wood = C(0x5a4636), wood_dark = C(0x3e3026);
    rect(wx0 - 10, wy0 - 10, wx1 - wx0 + 20, 10, wood, 255);
    rect(wx0 - 10, wy1, wx1 - wx0 + 20, 14, wood, 255);
    rect(wx0 - 10, wy0, 10, wy1 - wy0, wood, 255);
    rect(wx1, wy0, 10, wy1 - wy0, wood, 255);
    rect(236, wy0, 8, wy1 - wy0, wood, 255);
    rect(wx0, 150, wx1 - wx0, 6, wood, 255);
    rect(wx0 - 10, wy1 + 12, wx1 - wx0 + 20, 3, wood_dark, 255);

    /* Curtains with folds. */
    for (int side = -1; side <= 1; side += 2) {
        vg_mat_t m = vg_translate(vg_identity(), 240, 0);
        m = vg_scale(m, (float)side, 1);
        vg_set_matrix(m);
        vg_path_begin();
        vg_move_to(-240, 0);
        vg_line_to(-150, 0);
        vg_cubic_to(-160, 90, -140, 200, -166, 330);
        vg_cubic_to(-172, 350, -190, 356, -200, 352);
        vg_cubic_to(-214, 348, -224, 352, -240, 356);
        vg_fill(C(0x8e3b4a), 255);
        vg_stroke_cubic(-170, 4, -176, 110, -168, 220, -184, 348, 3, 5, C(0x6e2a38), 255);
        vg_stroke_cubic(-200, 4, -204, 120, -198, 230, -210, 348, 3, 5, C(0x6e2a38), 255);
        vg_stroke_cubic(-156, 6, -162, 100, -150, 210, -170, 330, 1.5f, 2.5f, C(0xb0566a), 255);
        vg_set_matrix(vg_identity());
    }

    /* Floor planks. */
    for (int i = 0; i < 6; ++i) {
        float y = 372 + i * i * 4.0f + i * 10.0f;
        vg_stroke_line(0, y, 480, y, 1.2f, C(0x2a1e16), 255);
    }
    for (int i = -6; i <= 6; ++i)
        vg_stroke_line(240 + i * 30, 360, 240 + i * 90, 480, 1.0f, C(0x3a2a1f), 255);
}

/* ------------------------------------------------------------------ */
/* Character                                                          */
/* ------------------------------------------------------------------ */

static void draw_twin_tail(float s, float angle)
{
    vg_mat_t m = vg_translate(head_layer(-6), s * 66, -150);
    m = vg_rotate(m, DEG(angle + s * 7));
    m = vg_scale(m, s, 1);
    vg_set_matrix(m);
    vg_path_begin();
    vg_move_to(-11, 0);
    vg_cubic_to(-32, 68, -34, 176, -18, 256);
    vg_cubic_to(-12, 290, -2, 313, 8, 332);
    vg_cubic_to(10, 303, 16, 270, 20, 230);
    vg_cubic_to(28, 150, 24, 54, 12, 0);
    vg_fill(HAIR, 255);
    vg_stroke_cubic(-4, 24, -16, 110, -14, 200, -2, 296, 2.2f, 0.6f, HAIR_DARK, 255);
    vg_stroke_cubic(8, 30, 12, 120, 12, 200, 6, 280, 1.8f, 0.5f, HAIR_DARK, 255);
    vg_stroke_cubic(-10, 40, -20, 100, -20, 150, -14, 200, 1.6f, 0.4f, HAIR_LIGHT, 255);
}

static void draw_back_hair(float sway)
{
    vg_mat_t m = vg_translate(head_layer(-10), 0, -120);
    m = vg_rotate(m, DEG(sway * 0.3f));
    m = vg_translate(m, 0, 120);
    vg_set_matrix(m);
    vg_path_begin();
    vg_move_to(-72, -120);
    vg_cubic_to(-80, -196, 80, -196, 72, -120);
    vg_cubic_to(80, -70, 86, -6, 80, 46);
    vg_cubic_to(52, 60, -52, 60, -80, 46);
    vg_cubic_to(-86, -6, -80, -70, -72, -120);
    vg_fill(HAIR_BACK, 255);
}

static void draw_body(vg_mat_t body)
{
    vg_set_matrix(body);
    /* Pleated skirt. */
    vg_path_begin();
    vg_move_to(-70, 196);
    vg_line_to(70, 196);
    vg_line_to(96, 262);
    vg_cubic_to(60, 272, -60, 272, -96, 262);
    vg_fill(SKIRT, 255);
    for (int i = -4; i <= 4; ++i)
        vg_stroke_line(i * 17.0f, 200, i * 23.0f, 268, 1.4f, NAVY_DARK, 255);
    vg_stroke_cubic(-94, 253, -50, 262, 50, 262, 94, 253, 2.2f, 2.2f, STRIPE, 255);
    vg_stroke_cubic(-91, 246, -50, 255, 50, 255, 91, 246, 1.4f, 1.4f, STRIPE, 255);

    /* Cardigan torso; the sleeves are separate limbs. */
    vg_path_begin();
    vg_move_to(-22, 4);
    vg_cubic_to(-40, 10, -54, 14, -62, 24);
    vg_cubic_to(-70, 40, -68, 90, -66, 140);
    vg_line_to(-68, 206);
    vg_cubic_to(-30, 213, 30, 213, 68, 206);
    vg_line_to(66, 140);
    vg_cubic_to(68, 90, 70, 40, 62, 24);
    vg_cubic_to(54, 14, 40, 10, 22, 4);
    vg_fill(CARDIGAN, 255);
    for (int s = -1; s <= 1; s += 2) {
        vg_stroke_cubic(s * 56.0f, 166, s * 48.0f, 168, s * 38.0f, 168, s * 28.0f, 166,
                        1.8f, 1.8f, CARDIGAN_SHADE, 255);
        vg_stroke_cubic(s * 58.0f, 60, s * 60.0f, 100, s * 60.0f, 150, s * 62.0f, 196,
                        0.8f, 1.8f, CARDIGAN_SHADE, 255);
    }
    vg_stroke_line(0, 96, 0, 206, 1.6f, CARDIGAN_SHADE, 255);

    /* Ribbed hem. */
    vg_path_begin();
    vg_move_to(-68, 192);
    vg_cubic_to(-30, 199, 30, 199, 68, 192);
    vg_line_to(68, 208);
    vg_cubic_to(30, 215, -30, 215, -68, 208);
    vg_fill(CARDIGAN_RIB, 255);
    for (float x = -63; x <= 63; x += 6) {
        float bow = 5 * (1 - (x / 68) * (x / 68));
        vg_stroke_line(x, 193 + bow, x, 207 + bow, 1.1f, CARDIGAN_SHADE, 255);
    }

    /* Buttons. */
    for (int i = 0; i < 4; ++i) {
        float y = 112 + i * 22.0f;
        vg_path_begin();
        vg_ellipse(-6, y, 3.6f, 3.6f);
        vg_fill(BUTTON, 255);
        vg_path_begin();
        vg_ellipse(-7, y - 1.2f, 1.2f, 1.2f);
        vg_fill(BUTTON_HI, 255);
    }

    /* Neck, shirt and sailor collar. */
    /* Skinned neck: top follows the head, bottom the body, middle blends. */
    static const float neck[3][2] = {{12, -44}, {13, -12}, {16, 12}};
    float nx[6], ny[6];
    for (int i = 0; i < 3; ++i) {
        float hx, hy, bx, by;
        for (int side = 0; side < 2; ++side) {
            float x = side ? neck[i][0] : -neck[i][0], y = neck[i][1];
            vg_apply(s_head, x, y, &hx, &hy);
            vg_apply(body, x, y, &bx, &by);
            float w = i == 0 ? 1.0f : i == 1 ? 0.5f : 0.0f;
            int slot = side ? 5 - i : i;
            nx[slot] = bx + (hx - bx) * w;
            ny[slot] = by + (hy - by) * w;
        }
    }
    vg_set_matrix(vg_identity());
    vg_path_begin();
    vg_move_to(nx[0], ny[0]);
    vg_line_to(nx[1], ny[1]);
    vg_line_to(nx[2], ny[2]);
    vg_line_to(nx[3], ny[3]);
    vg_line_to(nx[4], ny[4]);
    vg_line_to(nx[5], ny[5]);
    vg_fill(SKIN, 255);
    /* Chin shadow rides with the head; the face covers its upper half. */
    vg_set_matrix(head_layer(4));
    vg_path_begin();
    vg_move_to(-16, -40);
    vg_cubic_to(-12, -12, 12, -12, 16, -40);
    vg_fill(SKIN_SHADE, 255);
    vg_set_matrix(body);
    vg_path_begin();
    vg_move_to(-22, 4);
    vg_line_to(0, 80);
    vg_line_to(22, 4);
    vg_fill(SHIRT, 255);
    vg_path_begin();
    vg_move_to(-22, 2);
    vg_line_to(0, 86);
    vg_line_to(22, 2);
    vg_cubic_to(46, 6, 66, 16, 78, 32);
    vg_cubic_to(72, 52, 56, 72, 0, 112);
    vg_cubic_to(-56, 72, -72, 52, -78, 32);
    vg_cubic_to(-66, 16, -46, 6, -22, 2);
    vg_fill(NAVY, 255);
    for (int s = -1; s <= 1; s += 2) {
        vg_stroke_cubic(s * 71.0f, 36, s * 64.0f, 52, s * 50.0f, 70, s * 4.0f, 104,
                        1.8f, 1.8f, STRIPE, 255);
        vg_stroke_cubic(s * 66.0f, 34, s * 60.0f, 48, s * 46.0f, 64, s * 4.0f, 97,
                        1.2f, 1.2f, STRIPE, 255);
    }

    /* Ribbon tie. */
    for (int s = -1; s <= 1; s += 2) {
        vg_path_begin();
        vg_move_to(0, 84);
        vg_cubic_to(s * -10.0f + s * 20, 70, s * 30.0f, 72, s * 27.0f, 86);
        vg_cubic_to(s * 25.0f, 98, s * 10.0f, 96, 0, 86);
        vg_fill(TIE, 255);
        vg_path_begin();
        vg_move_to(s * 3.0f, 88);
        vg_line_to(s * 15.0f, 126);
        vg_line_to(s * 7.0f, 122);
        vg_line_to(s * 1.0f, 92);
        vg_fill(TIE_DARK, 255);
    }
    vg_path_begin();
    vg_ellipse(0, 86, 5.5f, 4.5f);
    vg_fill(TIE_DARK, 255);
}

static void draw_eye(float s, float open, float smile, float ex, float ey)
{
    float squash = 1 - 0.2f * s_yaw * s;
    float cx = s * 30 * squash + s_yaw * 14;
    float cy = -92 + s_pitch * 8;
    vg_mat_t eye = vg_translate(s_head, cx, cy);
    eye = vg_scale(eye, squash * s, 1);
    vg_set_matrix(eye);

    if (smile > 0.55f) {
        vg_stroke_cubic(-18, 4, -8, -12, 8, -12, 19, 2, 2.2f, 4.4f, LASH, 255);
        return;
    }
    float o = clampf(open * (1 - smile * 0.7f), 0, 1.35f);
    if (o < 0.12f) {
        vg_stroke_cubic(-18, 4, -8, 11, 8, 11, 20, 3, 2.4f, 4.6f, LASH, 255);
        vg_stroke_cubic(18, 5, 22, 6, 25, 9, 27, 12, 2.4f, 0.8f, LASH, 255);
        return;
    }

    vg_mat_t lid = vg_translate(eye, 0, 18);
    lid = vg_scale(lid, 1, o);
    lid = vg_translate(lid, 0, -18);
    vg_set_matrix(lid);
    vg_path_begin();
    vg_move_to(-19, 0);
    vg_cubic_to(-19, -14, -10, -22, 0, -22);
    vg_cubic_to(10, -22, 19, -14, 19, -2);
    vg_cubic_to(19, 10, 10, 18, 0, 18);
    vg_cubic_to(-10, 18, -19, 10, -19, 0);
    vg_fill(SCLERA, 255);

    float ix = ex * 6 * s, iy = 1 + ey * 2;
    vg_path_begin();
    vg_ellipse(ix, iy, 13, 15.5f);
    vg_fill(IRIS_OUT, 255);
    vg_path_begin();
    vg_ellipse(ix, iy + 0.5f, 11.2f, 13.6f);
    vg_fill(IRIS, 255);
    vg_path_begin();
    vg_ellipse(ix, iy + 7, 8.5f, 6.5f);
    vg_fill(IRIS_GLOW, 255);
    vg_path_begin();
    vg_ellipse(ix, iy - 11, 12, 5.5f);
    vg_fill(IRIS_OUT, 255);
    vg_path_begin();
    vg_ellipse(ix, iy - 1, 5.5f, 7.5f);
    vg_fill(PUPIL, 255);
    float hx = -5 * s;
    vg_path_begin();
    vg_ellipse(ix + hx, iy - 6, 4.6f, 5.2f);
    vg_fill(WHITE, 255);
    vg_path_begin();
    vg_ellipse(ix - hx * 0.9f, iy + 6, 2.2f, 2.2f);
    vg_fill(WHITE, 230);

    vg_set_matrix(eye);
    float top = 18 - 40 * o;
    vg_stroke_cubic(-20, top + 11 * o, -12, top - 1, 8, top - 2, 22, top + 7,
                    2.4f, 5.6f, LASH, 255);
    vg_stroke_cubic(20, top + 6, 24, top + 5, 27, top + 2, 30, top - 3, 3.2f, 0.8f, LASH, 255);
    vg_stroke_cubic(-14, top - 5, -4, top - 9, 8, top - 9, 18, top - 3, 0.6f, 1.4f,
                    SKIN_LINE, 255);
    vg_stroke_cubic(-12, 17.5f, -4, 19.5f, 6, 19.5f, 15, 16, 0.8f, 1.8f, C(0x7a5a58), 255);
}

static void draw_brow(float s, float brow)
{
    vg_set_matrix(head_layer(12));
    float y = -124 - brow * 5;
    float inner = brow < 0 ? -brow * 6 : -brow * 2;
    vg_stroke_cubic(s * 14, y + inner, s * 22, y - 3, s * 34, y - 4, s * 46, y + 2,
                    3.0f, 1.2f, BROW, 210);
}

static void draw_mouth(float open, float smile)
{
    vg_mat_t m = vg_translate(head_layer(12), 0, -50 + s_pitch * 4);
    vg_set_matrix(m);
    float w = 9 + smile * 3 - open;
    if (open < 0.08f) {
        vg_stroke_cubic(-w, -smile * 3, -w / 3, 1 + smile * 3, w / 3, 1 + smile * 3, w,
                        -smile * 3, 1.5f, 1.5f, LIP, 255);
        return;
    }
    float h = 2 + open * 12, top = -h * 0.3f - smile, bot = h * 0.7f, corner = -smile * 2;
    vg_path_begin();
    vg_move_to(-w, corner);
    vg_cubic_to(-w * 0.5f, top, w * 0.5f, top, w, corner);
    vg_cubic_to(w * 0.8f, bot, -w * 0.8f, bot, -w, corner);
    vg_fill(MOUTH, 255);
    vg_path_begin();
    vg_ellipse(0, h * 0.3f, w * 0.45f, h * 0.18f);
    vg_fill(TONGUE, 255);
}

static void draw_bangs(float sway)
{
    vg_set_matrix(head_layer(16));
    static const float tips[][2] = {{56, -100}, {30, -106}, {4, -110}, {-22, -106}, {-50, -100}};
    vg_path_begin();
    vg_move_to(-74, -112);
    vg_cubic_to(-82, -178, -30, -198, 0, -196);
    vg_cubic_to(30, -198, 82, -178, 74, -112);
    float px = 74, py = -112;
    for (unsigned i = 0; i < sizeof(tips) / sizeof(tips[0]); ++i) {
        float tx = tips[i][0], ty = tips[i][1];
        vg_quad_to(px - 2, (py + ty) * 0.5f - 6, tx, ty);
        float nx = i + 1 < sizeof(tips) / sizeof(tips[0]) ? (tx + tips[i + 1][0]) * 0.5f : -74;
        float ny = i + 1 < sizeof(tips) / sizeof(tips[0]) ? -134 : -112;
        vg_quad_to(tx - 3, ty - 14, nx, ny);
        px = nx;
        py = ny;
    }
    vg_fill(HAIR, 255);
    vg_path_begin();
    vg_move_to(-52, -164);
    vg_cubic_to(-24, -178, 24, -178, 52, -164);
    vg_cubic_to(24, -171, -24, -171, -52, -164);
    vg_fill(HAIR_LIGHT, 255);
    vg_stroke_cubic(-12, -186, -16, -160, -20, -140, -22, -112, 1.4f, 0.5f, HAIR_DARK, 255);
    vg_stroke_cubic(16, -186, 18, -160, 22, -140, 28, -110, 1.4f, 0.5f, HAIR_DARK, 255);
    vg_stroke_cubic(-40, -176, -46, -156, -48, -130, -46, -106, 1.2f, 0.4f, HAIR_DARK, 255);
    vg_stroke_cubic(42, -176, 48, -156, 50, -130, 52, -106, 1.2f, 0.4f, HAIR_DARK, 255);

    for (int s = -1; s <= 1; s += 2) {
        vg_mat_t m = vg_translate(head_layer(10), s * 60.0f, -150);
        m = vg_rotate(m, DEG(sway * 0.5f));
        m = vg_scale(m, (float)s, 1);
        vg_set_matrix(m);
        vg_path_begin();
        vg_move_to(-6, 0);
        vg_cubic_to(18, 30, 20, 80, 12, 118);
        vg_cubic_to(6, 94, 0, 56, -14, 22);
        vg_fill(HAIR, 255);
        vg_stroke_cubic(2, 12, 10, 40, 12, 80, 10, 108, 1.2f, 0.4f, HAIR_DARK, 255);
    }
}

static void draw_ribbon(float s, float tail)
{
    vg_mat_t m = vg_translate(head_layer(-2), s * 66, -150);
    m = vg_rotate(m, DEG(tail * 0.3f));
    m = vg_scale(m, s, 1);
    vg_set_matrix(m);
    vg_path_begin();
    vg_move_to(0, 0);
    vg_cubic_to(6, -24, 28, -22, 24, -4);
    vg_cubic_to(22, 6, 8, 5, 0, 0);
    vg_fill(RIBBON, 255);
    vg_path_begin();
    vg_move_to(0, 0);
    vg_cubic_to(-14, -20, -28, -8, -20, 6);
    vg_cubic_to(-14, 13, -4, 7, 0, 0);
    vg_fill(RIBBON, 255);
    vg_path_begin();
    vg_move_to(0, 2);
    vg_line_to(8, 30);
    vg_line_to(15, 25);
    vg_line_to(4, 0);
    vg_fill(RIBBON_DARK, 255);
    vg_stroke_cubic(4, -3, 10, -12, 16, -13, 20, -8, 1.2f, 0.5f, RIBBON_DARK, 255);
    vg_path_begin();
    vg_ellipse(0, 0, 5, 5.5f);
    vg_fill(RIBBON_DARK, 255);
}

static void draw_head(const puppet_pose_t *pose)
{
    const float *p = pose->p;
    /* Face and neck shadow. */
    vg_set_matrix(head_layer(4));
    float chin = s_pitch * 3;
    vg_path_begin();
    vg_move_to(-62, -125);
    vg_cubic_to(-64, -80, -48, -46 + chin, -14, -30 + chin);
    vg_cubic_to(-6, -26 + chin, 6, -26 + chin, 14, -30 + chin);
    vg_cubic_to(48, -46 + chin, 64, -80, 62, -125);
    vg_cubic_to(60, -176, -60, -176, -62, -125);
    vg_fill(SKIN, 255);

    float blush = clampf(p[PUPPET_BLUSH], 0, 1);
    for (int s = -1; s <= 1; s += 2) {
        vg_set_matrix(head_layer(10));
        vg_path_begin();
        vg_ellipse(s * 38.0f, -66, 14, 6.5f);
        vg_fill(BLUSH, (uint8_t)(40 + blush * 90));
        if (blush > 0.4f) {
            for (int i = 0; i < 3; ++i) {
                float x = s * 30.0f + i * 7 * s;
                vg_stroke_line(x + 3, -70, x - 1, -62, 1.3f, BLUSH_LINE,
                               (uint8_t)((blush - 0.4f) * 350));
            }
        }
    }

    draw_eye(-1, p[PUPPET_EYE_OPEN_L], p[PUPPET_EYE_SMILE], p[PUPPET_EYE_X], p[PUPPET_EYE_Y]);
    draw_eye(1, p[PUPPET_EYE_OPEN_R], p[PUPPET_EYE_SMILE], p[PUPPET_EYE_X], p[PUPPET_EYE_Y]);

    vg_set_matrix(head_layer(18));
    vg_stroke_line(-1, -73, 1, -66, 1.5f, SKIN_LINE, 255);
    draw_mouth(clampf(p[PUPPET_MOUTH_OPEN], 0, 1), clampf(p[PUPPET_MOUTH_SMILE], -1, 1));

    draw_bangs(pose->hair_sway);
    draw_brow(-1, p[PUPPET_BROW]);
    draw_brow(1, p[PUPPET_BROW]);
    draw_ribbon(-1, pose->tail_l);
    draw_ribbon(1, pose->tail_r);

    vg_mat_t m = vg_translate(head_layer(16), 4, -192);
    m = vg_rotate(m, DEG(pose->ahoge));
    vg_set_matrix(m);
    vg_stroke_cubic(0, 0, 8, -22, 26, -24, 22, -8, 3.4f, 0.6f, HAIR, 255);
}

/* ------------------------------------------------------------------ */
/* Limbs                                                              */
/* ------------------------------------------------------------------ */

static vg_mat_t s_body;
static float s_hit_x[PUPPET_LIMB_COUNT], s_hit_y[PUPPET_LIMB_COUNT];

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

/* Analytic two-bone IK; the joint bends toward `side` on x. */
static void solve_ik(float rx, float ry, float tx, float ty, float a, float b, float side,
                     float *jx, float *jy, float *ex, float *ey)
{
    float dx = tx - rx, dy = ty - ry, d = sqrtf(dx * dx + dy * dy);
    if (d < 1e-3f) { dx = 0; dy = 1; d = 1; }
    float ux = dx / d, uy = dy / d;
    float dc = clampf(d, fabsf(a - b) + 1, a + b - 0.5f);
    *ex = rx + ux * dc;
    *ey = ry + uy * dc;
    float ca = clampf((a * a + dc * dc - b * b) / (2 * a * dc), -1, 1);
    float sa = sqrtf(1 - ca * ca);
    float j1x = rx + a * (ux * ca - uy * sa), j1y = ry + a * (uy * ca + ux * sa);
    float j2x = rx + a * (ux * ca + uy * sa), j2y = ry + a * (uy * ca - ux * sa);
    bool first = (j1x - j2x) * side >= 0;
    *jx = first ? j1x : j2x;
    *jy = first ? j1y : j2y;
}

/* Local hand frame: +y toward the fingertips, thumb on -x. */
static void hand_shape(int gesture, float grow)
{
    const float g2 = grow * 2;
    vg_path_begin();
    vg_ellipse(0, 9, 8.5f + grow, 9.5f + grow);
    if (gesture == PUPPET_HAND_OPEN) {
        for (int i = 0; i < 4; ++i) {
            float x = -5.8f + i * 3.9f;
            add_capsule(x, 14, 3.4f + g2, x * 1.3f, 27 - fabsf(i - 1.5f) * 2.5f, 3.2f + g2);
        }
        add_capsule(-7, 7, 4.2f + g2, -13, 15, 3.6f + g2);
        return;
    }
    add_capsule(gesture == PUPPET_HAND_FIST ? -6.0f : 1.5f, 16, 5 + g2, 6, 16, 5 + g2);
    if (gesture == PUPPET_HAND_PEACE) {
        add_capsule(-3, 14, 3.6f + g2, -7, 32, 3.3f + g2);
        add_capsule(1, 14, 3.6f + g2, 5, 32, 3.3f + g2);
    } else if (gesture == PUPPET_HAND_POINT) {
        add_capsule(-3, 14, 3.6f + g2, -3.5f, 34, 3.3f + g2);
        add_capsule(-3, 16, 5 + g2, 1, 16, 5 + g2);
    }
    add_capsule(-8, 8, 4.4f + g2, -1, 14, 4 + g2);
}

static void draw_hand(float s, float x, float y, float ux, float uy, int gesture, int limb)
{
    vg_mat_t m = vg_translate(s_body, x, y);
    m = vg_rotate(m, atan2f(uy, ux) - 1.5707963f);
    m = vg_scale(m, s, 1);
    vg_set_matrix(m);
    hand_shape(gesture, 1.0f);
    vg_fill(SKIN_LINE, 255);
    hand_shape(gesture, 0.0f);
    vg_fill(SKIN, 255);
    vg_apply(m, 0, 10, &s_hit_x[limb], &s_hit_y[limb]);
}

static void draw_arm(const puppet_pose_t *pose, int limb)
{
    float s = limb == PUPPET_LIMB_ARM_L ? -1.0f : 1.0f;
    float sx = s * PUPPET_SHOULDER_X, sy = PUPPET_SHOULDER_Y, jx, jy, ex, ey;
    solve_ik(sx, sy, pose->limb_x[limb], pose->limb_y[limb], PUPPET_UPPER_ARM,
             PUPPET_FOREARM, s, &jx, &jy, &ex, &ey);
    float fx = ex - jx, fy = ey - jy, flen = sqrtf(fx * fx + fy * fy);
    float ux = flen > 1e-3f ? fx / flen : 0, uy = flen > 1e-3f ? fy / flen : 1;
    float cx0 = jx + fx * 0.7f, cy0 = jy + fy * 0.7f;
    float cx1 = jx + fx * 0.9f, cy1 = jy + fy * 0.9f;

    vg_set_matrix(s_body);
    vg_path_begin();
    add_capsule(sx, sy, 24, jx, jy, 19);
    add_capsule(jx, jy, 19, cx0, cy0, 17);
    vg_fill(CARDIGAN, 255);
    float ax = jx - sx, ay = jy - sy, alen = sqrtf(ax * ax + ay * ay);
    if (alen > 1e-3f) {
        float nx = -ay / alen, ny = ax / alen;
        if (nx * s > 0) { nx = -nx; ny = -ny; }
        vg_stroke_line(sx + ax * 0.35f + nx * 5, sy + ay * 0.35f + ny * 5,
                       jx + nx * 4, jy + ny * 4, 1.4f, CARDIGAN_SHADE, 255);
    }
    draw_hand(s, jx + fx * 0.86f, jy + fy * 0.86f, ux, uy,
              pose->gesture[limb == PUPPET_LIMB_ARM_L ? 0 : 1], limb);
    vg_set_matrix(s_body);
    vg_path_begin();
    add_capsule(cx0, cy0, 18, cx1, cy1, 18);
    vg_fill(CARDIGAN_RIB, 255);
    vg_stroke_line(cx0 + (cx1 - cx0) * 0.5f - uy * 8, cy0 + (cy1 - cy0) * 0.5f + ux * 8,
                   cx0 + (cx1 - cx0) * 0.5f + uy * 8, cy0 + (cy1 - cy0) * 0.5f - ux * 8,
                   1.0f, CARDIGAN_SHADE, 255);
}

static void draw_leg(const puppet_pose_t *pose, int limb)
{
    float s = limb == PUPPET_LIMB_LEG_L ? -1.0f : 1.0f;
    float hx = s * PUPPET_HIP_X, hy = PUPPET_HIP_Y, kx, ky, ax, ay;
    solve_ik(hx, hy, pose->limb_x[limb], pose->limb_y[limb], PUPPET_THIGH, PUPPET_SHIN,
             s, &kx, &ky, &ax, &ay);
    vg_set_matrix(s_body);
    vg_path_begin();
    add_capsule(hx, hy, 28, kx, ky, 20);
    add_capsule(kx, ky, 20, ax, ay, 14);
    vg_fill(SKIN, 255);
    float sx0 = kx + (ax - kx) * 0.38f, sy0 = ky + (ay - ky) * 0.38f;
    vg_path_begin();
    add_capsule(sx0, sy0, 19, ax, ay, 15.5f);
    vg_fill(C(0xf2f3f8), 255);
    vg_stroke_line(sx0 - 9.5f, sy0, sx0 + 9.5f, sy0, 1.2f, C(0xc9ccd8), 255);
    vg_stroke_line(kx - 4, ky - 2, kx + 4, ky - 2, 1.2f, SKIN_SHADE, 255);

    vg_mat_t m = vg_translate(s_body, ax, ay);
    m = vg_scale(m, s, 1);
    vg_set_matrix(m);
    vg_path_begin();
    vg_move_to(-9, -2);
    vg_cubic_to(-9, -9, 6, -9, 10, -3);
    vg_cubic_to(18, 0, 20, 6, 16, 9);
    vg_line_to(-9, 9);
    vg_cubic_to(-12, 6, -12, 1, -9, -2);
    vg_fill(C(0x3e2a22), 255);
    vg_stroke_line(-9, 9, 16, 9, 2.2f, C(0x241812), 255);
    vg_path_begin();
    vg_ellipse(6, -2.5f, 3.5f, 1.4f);
    vg_fill(C(0x76584a), 255);
    vg_apply(m, 2, 0, &s_hit_x[limb], &s_hit_y[limb]);
}

static void draw_floor_shadow(const puppet_pose_t *pose)
{
    float x = (pose->limb_x[PUPPET_LIMB_LEG_L] + pose->limb_x[PUPPET_LIMB_LEG_R]) * 0.5f;
    float y = fmaxf(pose->limb_y[PUPPET_LIMB_LEG_L], pose->limb_y[PUPPET_LIMB_LEG_R]);
    vg_set_matrix(s_body);
    vg_path_begin();
    vg_ellipse(x, y + 10, 62, 9);
    vg_fill(C(0x120c0a), 110);
}

static bool hand_near_face(const puppet_pose_t *pose, int limb)
{
    return pose->limb_y[limb] < -10 && fabsf(pose->limb_x[limb]) < 80;
}

int puppet_draw_hit_limb(float x, float y, float radius)
{
    int best = -1;
    float best_d = radius * radius;
    for (int i = 0; i < PUPPET_LIMB_COUNT; ++i) {
        float dx = x - s_hit_x[i], dy = y - s_hit_y[i], d = dx * dx + dy * dy;
        if (d < best_d) { best_d = d; best = i; }
    }
    return best;
}

bool puppet_draw_screen_to_body(float x, float y, float *bx, float *by)
{
    const vg_mat_t m = s_body;
    float det = m.a * m.d - m.b * m.c;
    if (fabsf(det) < 1e-6f) return false;
    float px = x - m.e, py = y - m.f;
    *bx = (m.d * px - m.c * py) / det;
    *by = (-m.b * px + m.a * py) / det;
    return true;
}

static vg_mat_t body_matrix(const puppet_pose_t *pose, float neck_x, float neck_y, float zoom)
{
    const float *p = pose->p;
    /* Zoom about the face so it stays on screen. */
    vg_mat_t body = vg_translate(vg_identity(), neck_x, neck_y - PUPPET_FACE_ABOVE_NECK);
    body = vg_scale(body, zoom, zoom);
    body = vg_translate(body, 0, PUPPET_FACE_ABOVE_NECK + p[PUPPET_BODY_BOUNCE]);
    /* The torso takes a share of the head roll so the neck never pivots alone. */
    body = vg_translate(body, 0, 200);
    body = vg_rotate(body, DEG(p[PUPPET_BODY_LEAN] * 4 + p[PUPPET_HEAD_ROLL] * 0.18f));
    return vg_translate(body, p[PUPPET_BODY_LEAN] * 6, -200);
}

void puppet_draw_layout(const puppet_pose_t *pose, float neck_x, float neck_y, float zoom)
{
    s_body = body_matrix(pose, neck_x, neck_y, zoom);
    for (int i = 0; i < PUPPET_LIMB_COUNT; i++)
        vg_apply(s_body, pose->limb_x[i], pose->limb_y[i], &s_hit_x[i], &s_hit_y[i]);
}

void puppet_draw_character(const puppet_pose_t *pose, float neck_x, float neck_y, float zoom)
{
    const float *p = pose->p;
    s_yaw = clampf(p[PUPPET_HEAD_YAW], -1, 1);
    s_pitch = clampf(p[PUPPET_HEAD_PITCH], -1, 1);

    /* Zoom about the face so it stays on screen. */
    vg_mat_t body = vg_translate(vg_identity(), neck_x, neck_y - PUPPET_FACE_ABOVE_NECK);
    body = vg_scale(body, zoom, zoom);
    body = vg_translate(body, 0, PUPPET_FACE_ABOVE_NECK + p[PUPPET_BODY_BOUNCE]);
    /* The torso takes a share of the head roll so the neck never pivots alone. */
    float roll = p[PUPPET_HEAD_ROLL];
    body = vg_translate(body, 0, 200);
    body = vg_rotate(body, DEG(p[PUPPET_BODY_LEAN] * 4 + roll * 0.18f));
    body = vg_translate(body, p[PUPPET_BODY_LEAN] * 6, -200);
    vg_mat_t head = vg_translate(body, 0, 8 - pose->breath * 1.5f);
    head = vg_rotate(head, DEG(roll * 0.82f));
    head = vg_translate(head, 0, -16);
    s_head = vg_translate(head, 0, s_pitch * 4);

    s_body = body;
    draw_twin_tail(-1, pose->tail_l);
    draw_twin_tail(1, pose->tail_r);
    draw_back_hair(pose->hair_sway);
    draw_floor_shadow(pose);
    draw_leg(pose, PUPPET_LIMB_LEG_L);
    draw_leg(pose, PUPPET_LIMB_LEG_R);
    draw_body(body);
    bool front_l = hand_near_face(pose, PUPPET_LIMB_ARM_L);
    bool front_r = hand_near_face(pose, PUPPET_LIMB_ARM_R);
    if (!front_l) draw_arm(pose, PUPPET_LIMB_ARM_L);
    if (!front_r) draw_arm(pose, PUPPET_LIMB_ARM_R);
    draw_head(pose);
    if (front_l) draw_arm(pose, PUPPET_LIMB_ARM_L);
    if (front_r) draw_arm(pose, PUPPET_LIMB_ARM_R);
    vg_set_matrix(vg_identity());
}

/* ------------------------------------------------------------------ */
/* Petals                                                             */
/* ------------------------------------------------------------------ */

static uint32_t s_petal_seed;
static float petal_rand(void)
{
    s_petal_seed = s_petal_seed * 1664525U + 1013904223U;
    return (float)(s_petal_seed >> 8) * (1.0f / 16777216.0f);
}

static void petal_spawn(puppet_petal_t *p, bool top)
{
    p->x = petal_rand() * 520 - 20;
    p->y = top ? -12 - petal_rand() * 40 : petal_rand() * 480;
    p->vx = 8 + petal_rand() * 18;
    p->vy = 22 + petal_rand() * 30;
    p->rot = petal_rand() * 6.283f;
    p->vrot = (petal_rand() - 0.5f) * 4;
    p->front = petal_rand() < 0.3f;
    p->size = p->front ? 1.4f + petal_rand() * 0.7f : 0.7f + petal_rand() * 0.5f;
}

void puppet_petals_init(puppet_petal_t *petals, uint32_t seed)
{
    s_petal_seed = seed ? seed : 7;
    for (int i = 0; i < PUPPET_PETAL_COUNT; ++i) petal_spawn(&petals[i], false);
}

void puppet_petals_update(puppet_petal_t *petals, float dt, float wind)
{
    for (int i = 0; i < PUPPET_PETAL_COUNT; ++i) {
        puppet_petal_t *p = &petals[i];
        p->x += (p->vx + wind) * dt * p->size;
        p->y += p->vy * dt * p->size;
        p->x += sinf(p->rot) * 0.6f;
        p->rot += p->vrot * dt;
        if (p->y > 500 || p->x > 520) petal_spawn(p, true);
    }
}

void puppet_draw_petals(const puppet_petal_t *petals, bool front)
{
    for (int i = 0; i < PUPPET_PETAL_COUNT; ++i) {
        const puppet_petal_t *p = &petals[i];
        if (p->front != front) continue;
        vg_mat_t m = vg_translate(vg_identity(), p->x, p->y);
        m = vg_rotate(m, p->rot);
        m = vg_scale(m, p->size * (0.6f + 0.4f * fabsf(cosf(p->rot * 1.7f))), p->size);
        vg_set_matrix(m);
        vg_path_begin();
        vg_move_to(0, -6);
        vg_cubic_to(6, -5, 5, 4, 0, 6);
        vg_cubic_to(-5, 4, -6, -5, 0, -6);
        vg_fill(PETAL, 255);
        vg_stroke_line(0, -4, 0, 3, 0.8f, PETAL_DARK, 255);
    }
    vg_set_matrix(vg_identity());
}
