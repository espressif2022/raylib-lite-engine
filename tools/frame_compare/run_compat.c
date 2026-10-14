// SPDX-License-Identifier: Apache-2.0
#define FRAME_COMPARE_COMPAT 1
#include "frame_host.h"
#include "frame_scene.h"
#include "raylib_lite_2d.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_raylib_port.h"

#include <stdlib.h>

typedef struct {
    frame_surface_t *surface;
    Texture2D texture;
    uint16_t texels[FRAME_TEXTURE_SIZE * FRAME_TEXTURE_SIZE];
} compat_state_t;

raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    (void)name;
    (void)out;
    return RAYLIB_LITE_NOT_SUPPORTED;
}

raylib_lite_result_t raylib_lite_asset_open_id(raylib_lite_asset_id_t id, raylib_lite_asset_view_t *out)
{
    (void)id;
    (void)out;
    return RAYLIB_LITE_NOT_FOUND;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (!view) return;
    view->data = NULL;
    view->size = 0;
}

static int setup(frame_surface_t *surface, int width, int height, void **user,
                 long long *color_bytes, long long *depth_bytes)
{
    compat_state_t *state = calloc(1, sizeof(*state));
    if (!state) return -1;
    *user = state;
    state->surface = surface;
    frame_scene_fill_texture(state->texels, FRAME_TEXTURE_SIZE, FRAME_TEXTURE_SIZE);
    raylib_lite_video_backend_t backend = frame_surface_backend(surface);
    if (raylib_lite_raylib_port_init_backend(&backend) != RAYLIB_LITE_OK) return -1;
    InitWindow(width, height, "frame-compare");
    if (!IsWindowReady() || GetScreenWidth() != width || GetScreenHeight() != height) return -1;
    state->texture = raylib_lite_2d_register_rgb565(
        state->texels, FRAME_TEXTURE_SIZE, FRAME_TEXTURE_SIZE);
    *color_bytes = 0;
    *depth_bytes = 0;
    return state->texture.id ? 0 : -1;
}

static void draw(void *user, int scene, int width, int height)
{
    compat_state_t *state = user;
    BeginDrawing();
    frame_scene_draw(scene, width, height, state->texture);
    EndDrawing();
}

static uint64_t copy_ns(void *user)
{
    (void)user;
    return 0;
}

static void shutdown(void *user)
{
    compat_state_t *state = user;
    if (!state) return;
    if (state->texture.id) UnloadTexture(state->texture);
    CloseWindow();
    raylib_lite_raylib_port_deinit();
    free(state);
}

const frame_ops_t *frame_compare_ops(void)
{
    static const frame_ops_t ops = {
        .path = "compat",
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
