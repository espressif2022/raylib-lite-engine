// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_ui.h"
#include "raylib_lite_raylib_impl.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* The actual bitmap-font and renderer implementations draw into a framebuffer;
 * only the legacy facade and unused file loading are replaced here. */
raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{ (void)name; (void)out; return RAYLIB_LITE_NOT_FOUND; }
void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{ if (view) memset(view, 0, sizeof(*view)); }
static unsigned rectangles;
void raylib_lite_raylib_draw_rectangle(int x, int y, int w, int h, Color color)
{
    assert(x == 14 && y == 2 && w == 10 && h == 12);
    assert(color.r == 50 && color.g == 40 && color.b == 30 && color.a == 200);
    ++rectangles;
}
int raylib_lite_raylib_measure_text(const char *text, int size)
{ (void)text; (void)size; assert(!"bitmap UI must not measure with the default font"); return 0; }
void raylib_lite_raylib_draw_text(const char *text, int x, int y, int size, Color color)
{ (void)text; (void)x; (void)y; (void)size; (void)color; assert(!"bitmap UI must not use default glyphs"); }
static void u16(uint8_t *p, unsigned value) { p[0] = value; p[1] = value >> 8; }
static void u32(uint8_t *p, unsigned value) { u16(p, value); u16(p + 2, value >> 16); }
static void expect_glyph(uint16_t *pixels, int x, int y, bool chinese)
{
    // Independently specified two-column glyphs: ? left column, 中 right column.
    for (int row = 0; row < 2; ++row) {
        int column = x + (chinese ? 1 : 0);
        if (column >= 6 && column < 24 && y + row >= 0 && y + row < 16)
            pixels[(y + row) * 28 + column] = 65535;
    }
}
int main(void)
{
    uint8_t data[24 + 2 * 20] = {0};
    memcpy(data, "RLF1", 4); u16(data + 4, 4); u16(data + 6, 2);
    u16(data + 8, 4); u16(data + 10, 3); u32(data + 12, 2); u32(data + 16, 63);
    for (unsigned n = 0; n < 2; ++n) {
        uint8_t *g = data + 24 + n * 20;
        u32(g, n ? 0x4e2d : 63); u16(g + 4, n * 2);
        u16(g + 8, 2); u16(g + 10, 2); u16(g + 14, 1); u16(g + 16, 3);
    }
    const uint16_t atlas_pixels[] = {65535, 0, 0, 65535, 65535, 0, 0, 65535};
    raylib_lite_renderer_texture_t atlas = raylib_lite_renderer_register_rgb565(atlas_pixels, 4, 2);
    assert(atlas.id);
    raylib_lite_font_t *font = NULL;
    assert(raylib_lite_font_from_memory(data, sizeof(data), atlas, &font) == RAYLIB_LITE_OK);
    raylib_lite_ui_t ui; raylib_lite_ui_init(&ui);
    raylib_lite_ui_node_t label = {.type = RAYLIB_LITE_UI_LABEL, .bounds = {2, 2, 10, 8},
        .text = "\xe4\xb8\xad?", .text_color = {255, 255, 255, 255}};
    assert(raylib_lite_ui_add(&ui, label));
    label.type = RAYLIB_LITE_UI_BUTTON; label.bounds = (raylib_lite_renderer_rect_t){14, 2, 10, 12};
    label.text = "\xe4\xb8\xad\n?"; label.color = (raylib_lite_renderer_color_t){100, 80, 60, 200};
    assert(raylib_lite_ui_add(&ui, label)); ui.nodes[1].pressed = true;
    assert(raylib_lite_ui_add(&ui, label));
    ui.nodes[2].visible = false; ui.nodes[2].text = (const char *)1;
    ui.nodes[2].bounds.x = NAN; // hidden content must never be inspected
    uint16_t pixels[28 * 16] = {0}, expected[28 * 16] = {0};
    raylib_lite_renderer_set_target(pixels, 28, 28, 16);
    raylib_lite_renderer_set_clip(6, 0, 18, 16);
    assert(raylib_lite_ui_draw_font(&ui, font, 0) == RAYLIB_LITE_OK);
    expect_glyph(expected, 4, 5, true); expect_glyph(expected, 7, 5, false);
    expect_glyph(expected, 17, 5, true); expect_glyph(expected, 17, 9, false);
    assert(!memcmp(pixels, expected, sizeof(pixels)) && rectangles == 1);
    memset(pixels, 0, sizeof(pixels)); memset(expected, 0, sizeof(expected));
    assert(raylib_lite_ui_draw_font(&ui, font, 3) == RAYLIB_LITE_OK);
    expect_glyph(expected, 5, 3, true); expect_glyph(expected, 5, 7, false);
    expect_glyph(expected, 17, 5, true); expect_glyph(expected, 17, 9, false);
    assert(!memcmp(pixels, expected, sizeof(pixels)) && rectangles == 2);
    raylib_lite_text_metrics_t metrics;
    assert(raylib_lite_font_measure(font, "?", 1, 0, &metrics) == RAYLIB_LITE_OK);
    assert(metrics.width == 3 && metrics.height == 4); // UI borrowed the live font
    assert(raylib_lite_ui_draw_font(NULL, font, 0) == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_ui_draw_font(&ui, NULL, 0) == RAYLIB_LITE_INVALID_ARGUMENT);
    ui.nodes[0].bounds.x = NAN;
    assert(raylib_lite_ui_draw_font(&ui, font, 0) == RAYLIB_LITE_INVALID_SIZE);
    ui.nodes[0].bounds.x = INFINITY;
    assert(raylib_lite_ui_draw_font(&ui, font, 0) == RAYLIB_LITE_INVALID_SIZE);
    ui.nodes[0].bounds.x = 0; ui.nodes[0].bounds.width = -1;
    assert(raylib_lite_ui_draw_font(&ui, font, 0) == RAYLIB_LITE_INVALID_SIZE);
    assert(rectangles == 2 && !memcmp(pixels, expected, sizeof(pixels)));
    raylib_lite_font_unload(font); // no UI teardown/font ownership operation required
    raylib_lite_renderer_unload_texture(atlas);
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);
    puts("ui bitmap font: ok");
    return 0;
}
