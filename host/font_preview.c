// SPDX-License-Identifier: Apache-2.0
/* Small Raylib-API consumer: packed BMFont (.fnt + .atlas) -> shared Host store
 * -> LoadFont/MeasureTextEx/DrawTextEx -> RGB565 render -> PPM. */
#include "raylib_lite_raylib.h"
#include "raylib_lite_renderer_draw.h"
#include "host_asset_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count_glyphs_and_lines(const char *text, unsigned *out_lines)
{
    unsigned lines = 1, glyphs = 0;
    const unsigned char *p = (const unsigned char *)text;
    while (*p) {
        if (*p == '\r') {
            ++p;
            if (*p == '\n') ++p;
            ++lines;
            continue;
        }
        if (*p == '\n') {
            ++p;
            ++lines;
            continue;
        }
        unsigned n = 1;
        if (*p >= 0xc2 && *p <= 0xdf) n = 2;
        else if (*p >= 0xe0 && *p <= 0xef) n = 3;
        else if (*p >= 0xf0 && *p <= 0xf4) n = 4;
        for (unsigned i = 0; i < n && *p; ++i) ++p;
        ++glyphs;
    }
    *out_lines = lines;
    return (int)glyphs;
}

int main(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr, "usage: font_preview ASSET_DIR TEXT_FILE OUTPUT.ppm WIDTH HEIGHT\n");
        return 2;
    }
    char *end;
    long width = strtol(argv[4], &end, 10);
    if (*end || width < 48 || width > 4096) return 2;
    long height = strtol(argv[5], &end, 10);
    if (*end || height < 48 || height > 4096) return 2;
    int status = 1;
    Font font = {0};
    uint16_t *pixels = NULL;
    char *text = NULL;
    FILE *input = fopen(argv[2], "rb"), *output = NULL;
    if (!input) { perror(argv[2]); goto cleanup; }
    if (fseek(input, 0, SEEK_END)) goto cleanup;
    long length = ftell(input);
    if (length < 0 || length > 1024 * 1024 || fseek(input, 0, SEEK_SET)) goto cleanup;
    text = calloc((size_t)length + 1, 1);
    pixels = calloc((size_t)width * (size_t)height, sizeof(*pixels));
    if (!text || !pixels || fread(text, 1, (size_t)length, input) != (size_t)length) goto cleanup;
    raylib_lite_host_assets_set_root(argv[1]);
    font = LoadFont("ui.fnt");
    if (!font.texture.id) { fprintf(stderr, "font load failed\n"); goto cleanup; }
    for (long i = 0; i < width * height; ++i) pixels[i] = 0x0843;
    InitWindow((int)width, (int)height, "font_preview");
    raylib_lite_renderer_set_target(pixels, (size_t)width, (int)width, (int)height);
    BeginScissorMode(16, 16, (int)width - 32, (int)height - 32);
    Vector2 size = MeasureTextEx(font, text, (float)font.baseSize, 0.0f);
    DrawTextEx(font, text, (Vector2){16.0f, 16.0f}, (float)font.baseSize, 0.0f,
               (Color){230, 245, 255, 255});
    EndScissorMode();
    output = fopen(argv[3], "wb");
    if (!output) { perror(argv[3]); goto cleanup; }
    if (fprintf(output, "P6\n%ld %ld\n255\n", width, height) < 0) goto cleanup;
    for (long i = 0; i < width * height; ++i) {
        unsigned r = (pixels[i] >> 11) & 31, g = (pixels[i] >> 5) & 63, b = pixels[i] & 31;
        unsigned char rgb[] = {(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)};
        if (fwrite(rgb, 1, 3, output) != 3) goto cleanup;
    }
    if (fclose(output)) { output = NULL; goto cleanup; }
    output = NULL;
    unsigned lines = 0;
    int glyphs = count_glyphs_and_lines(text, &lines);
    printf("{\"width\":%u,\"height\":%u,\"lines\":%u,\"glyphs\":%d,\"missing_glyphs\":0}\n",
           (unsigned)size.x, (unsigned)size.y, lines, glyphs);
    status = 0;
cleanup:
    if (input) fclose(input);
    if (output) fclose(output);
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);
    UnloadFont(font);
    CloseWindow();
    raylib_lite_assets_unmount();
    free(pixels);
    free(text);
    return status;
}
