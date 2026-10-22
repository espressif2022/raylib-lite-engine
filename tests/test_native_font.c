// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "frame_host.h"

static void check_font(const char *path, frame_surface_t *surface, int pages)
{
    Font font = LoadFont(path);
    assert(font.texture.id && font.baseSize == 8 && font.glyphCount == pages);
    for (int i = 0; i < pages; ++i) {
        Color color = GetImageColor(font.glyphs[i].image, 1, 1);
        assert(color.a == 255);
        assert(color.r == (i == 0 ? 255 : 0));
        assert(color.g == (i == 1 ? 255 : 0));
        assert(color.b == (i == 2 ? 255 : 0));
        assert(font.recs[i].y == i * 8);
    }
    if (pages >= 2) {
        /* The second glyph is U+4E2D: cover UTF-8 lookup and page stitching. */
        const char *text = "A\xe4\xb8\xad";
        assert(MeasureTextEx(font, text, 8, 0).x == 8);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawTextEx(font, text, (Vector2){1, 1}, 8, 0, WHITE);
        EndDrawing();
        assert(surface->pixels[2 * surface->stride + 2] == 0xf800);
        assert(surface->pixels[2 * surface->stride + 6] == 0x07e0);
    }
    UnloadFont(font);
}

int main(int argc, char **argv)
{
    assert(argc == 4);
    frame_surface_t surface;
    assert(frame_surface_init(&surface, 32, 16) == 0);
    raylib_lite_rcore_config_t config = {
        .video = frame_surface_backend(&surface), .clock = frame_host_clock(),
    };
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    SetTraceLogLevel(LOG_NONE);
    InitWindow(32, 16, "native-font-regression");
    assert(IsWindowReady());
    SetTargetFPS(0);
    for (int repeat = 0; repeat < 8; ++repeat) {
        for (int pages = 1; pages <= 3; ++pages) check_font(argv[pages], &surface, pages);
    }
    CloseWindow();
    frame_surface_free(&surface);
    puts("native font: ok");
    return 0;
}
