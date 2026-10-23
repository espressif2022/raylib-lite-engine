// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_raylib.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_renderer_draw.h"
#include "raylib_lite_renderer_stats.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *root;
static unsigned acquired, released;

/* Isolated file-asset adapter: every successful open has an owned view. */
raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *file = fopen(path, "rb");
    if (!file) return RAYLIB_LITE_NOT_FOUND;
    assert(fseek(file, 0, SEEK_END) == 0);
    long length = ftell(file);
    assert(length > 0 && fseek(file, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)length);
    assert(bytes && fread(bytes, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *out = (raylib_lite_asset_view_t){.data = bytes, .size = (size_t)length};
    ++acquired;
    return RAYLIB_LITE_OK;
}
void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (view && view->data) {
        free((void *)view->data);
        ++released;
        memset(view, 0, sizeof(*view));
    }
}

static void check_metrics(Font font)
{
    Vector2 m = MeasureTextEx(font, NULL, 5, 0);
    assert(m.x == 0 && m.y == 0);
    m = MeasureTextEx(font, "", 5, 0);
    assert(m.x == 0 && m.y == 0);
    const char text[] = "A\xe4\xb8\xad\n\xe6\x96\x87?";
    m = MeasureTextEx(font, text, 5, 0);
    assert(m.x == 6 && m.y == 12);
    m = MeasureTextEx(font, "A\r\nA\rA", 5, 0);
    assert(m.x == 3 && m.y == 19);
}

static void check_pixels_and_fastpath(Font font)
{
    uint16_t pixels[12 * 14] = {0}, expected[12 * 14] = {0};
    InitWindow(12, 14, "font-oracle");
    raylib_lite_renderer_set_target(pixels, 12, 12, 14);
    BeginScissorMode(2, 2, 4, 10);
    raylib_lite_renderer_reset_raster_stats();
    const char text[] = "A\xe4\xb8\xad\n\xe6\x96\x87!";
    DrawTextEx(font, text, (Vector2){1, 1}, 5, 0, WHITE);
    EndScissorMode();
    /* Verify that 1:1 WHITE draw uses the M2D_ATLAS_BINARY_ALPHA fast path! */
    raylib_lite_renderer_raster_stats_t stats = {0};
    raylib_lite_renderer_get_raster_stats(&stats);
    assert(stats.binary_copy_calls > 0 && stats.alpha_calls == 0);
    /* Independently specified 2x3 masks for ?, A, 中, 文, and the four pen positions.
     * Note: '!' is not in the font and must use the custom fallback U+4E2D (中, index 2),
     * not '?' (index 0) or 'A' (index 1). */
    const unsigned masks[] = {0x15, 0x2d, 0x3f, 0x36};
    const int glyphs[] = {1, 2, 3, 2}, px[] = {1, 4, 1, 4}, py[] = {2, 2, 9, 9};
    for (int n = 0; n < 4; ++n)
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 2; ++x) {
                int dx = px[n] + x, dy = py[n] + y;
                if (dx >= 2 && dx < 6 && dy >= 2 && dy < 12 &&
                    (masks[glyphs[n]] & (1U << (y * 2 + x))))
                    expected[dy * 12 + dx] = 65535;
            }
    assert(!memcmp(pixels, expected, sizeof(pixels)));
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);
    CloseWindow();
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    root = argv[1];
    Font font = LoadFont("ui.fnt");
    assert(font.texture.id && font.glyphCount > 0 && font.glyphPadding == 0);
    if (argc == 3) {
        /* Pipeline consumer: actual generated CJK subset, including a newline and fallback. */
        uint16_t pixels[128 * 64] = {0}, expected_fb[128 * 64] = {0}, actual_fb[128 * 64] = {0};
        const char text[] = "\xe4\xb8\xad\xe6\x96\x87\nA?";
        InitWindow(128, 64, "font-pipeline");
        raylib_lite_renderer_set_target(pixels, 128, 128, 64);
        raylib_lite_renderer_reset_raster_stats();
        Vector2 m = MeasureTextEx(font, text, (float)font.baseSize, 0);
        assert(m.x > 0 && m.y == font.baseSize * 2 + 2);
        DrawTextEx(font, text, (Vector2){2, 2}, (float)font.baseSize, 0, WHITE);
        raylib_lite_renderer_raster_stats_t stats = {0};
        raylib_lite_renderer_get_raster_stats(&stats);
        assert(stats.binary_copy_calls > 0 && stats.alpha_calls == 0);
        unsigned lit = 0;
        for (size_t i = 0; i < sizeof(pixels) / sizeof(*pixels); ++i) lit += pixels[i] != 0;
        assert(lit > 0);
        /* Unknown glyph '!' must render identically to configured fallback U+4E2D (中). */
        raylib_lite_renderer_set_target(expected_fb, 128, 128, 64);
        DrawTextEx(font, "\xe4\xb8\xad", (Vector2){2, 2}, (float)font.baseSize, 0, WHITE);
        raylib_lite_renderer_set_target(actual_fb, 128, 128, 64);
        DrawTextEx(font, "!", (Vector2){2, 2}, (float)font.baseSize, 0, WHITE);
        assert(!memcmp(expected_fb, actual_fb, sizeof(expected_fb)));
        raylib_lite_renderer_set_target(NULL, 0, 0, 0);
        CloseWindow();
    } else {
        check_metrics(font);
        check_pixels_and_fastpath(font);
    }
    UnloadFont(font);
    assert(acquired == released);
    Font missing = LoadFont("missing.fnt");
    assert(!missing.texture.id);
    UnloadFont(missing);
    assert(acquired == released);
    puts("bitmap font: ok");
    return 0;
}
