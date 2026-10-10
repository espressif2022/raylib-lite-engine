// SPDX-License-Identifier: Apache-2.0
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "frame_host.h"
#include "frame_scene.h"

#include <stdlib.h>
#ifdef ESP_PLATFORM
#include "esp_memory_utils.h"
#endif

extern void *swGetColorBuffer(int *width, int *height);

typedef struct {
    frame_surface_t *surface;
    Texture2D texture;
    uint16_t texels[FRAME_TEXTURE_SIZE * FRAME_TEXTURE_SIZE];
} upstream_state_t;

static Texture2D load_texture(const uint16_t *src, int width, int height)
{
    Color *pixels = calloc((size_t)width * (size_t)height, sizeof(Color));
    if (!pixels) return (Texture2D){ 0 };
    for (int i = 0; i < width * height; ++i) {
        unsigned value = src[i];
        unsigned r = (value >> 11) & 31u;
        unsigned g = (value >> 5) & 63u;
        unsigned b = value & 31u;
        pixels[i] = (Color){
            (unsigned char)((r << 3) | (r >> 2)),
            (unsigned char)((g << 2) | (g >> 4)),
            (unsigned char)((b << 3) | (b >> 2)),
            255,
        };
    }
    Image image = { pixels, width, height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

static int setup(frame_surface_t *surface, int width, int height, void **user,
                 long long *color_bytes, long long *depth_bytes)
{
    upstream_state_t *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    *user = state;
    state->surface = surface;
    frame_scene_fill_texture(state->texels, FRAME_TEXTURE_SIZE, FRAME_TEXTURE_SIZE);
    raylib_lite_rcore_config_t config = {
        .video = frame_surface_backend(surface),
        .clock = frame_host_clock(),
    };
    if (raylib_lite_rcore_configure(&config) != RAYLIB_LITE_OK) return -1;
    SetTraceLogLevel(LOG_NONE);
    InitWindow(width, height, "frame-compare");
    if (!IsWindowReady() || GetScreenWidth() != width || GetScreenHeight() != height) return -1;
    SetTargetFPS(0);
    int buffer_w = 0, buffer_h = 0;
    void *color = swGetColorBuffer(&buffer_w, &buffer_h);
    if (!color || buffer_w != width || buffer_h != height) return -1;
#ifdef ESP_PLATFORM
    frame_compare_color_psram = esp_ptr_external_ram(color) ? 1 : 0;
#endif
    *color_bytes = (long long)width * height * 2;
    *depth_bytes = (long long)width * height * 2;
    state->texture = load_texture(state->texels, FRAME_TEXTURE_SIZE, FRAME_TEXTURE_SIZE);
    return state->texture.id ? 0 : -1;
}

static void draw(void *user, int scene, int width, int height)
{
    upstream_state_t *state = user;
    BeginDrawing();
    frame_scene_draw(scene, width, height, state->texture);
    EndDrawing();
}

static uint64_t copy_ns(void *user)
{
    upstream_state_t *state = user;
    return state->surface->last_copy_ns;
}

static void shutdown(void *user)
{
    upstream_state_t *state = user;
    if (!state) return;
    if (state->texture.id) UnloadTexture(state->texture);
    CloseWindow();
    free(state);
}

const frame_ops_t *frame_compare_ops(void)
{
    static const frame_ops_t ops = {
        .path = "upstream",
        .setup = setup,
        .draw = draw,
        .copy_ns = copy_ns,
        .shutdown = shutdown,
    };
    return &ops;
}

#ifndef ESP_PLATFORM
int main(int argc, char **argv)
{
    return frame_host_main(argc, argv, frame_compare_ops());
}
#endif
