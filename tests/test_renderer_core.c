// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib_lite_renderer.h"

#define ATLAS_MAGIC 0x3141534dU
#define WALL_MAGIC 0x3157534dU

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, flags;
    uint32_t rgb_bytes, alpha_bytes;
    uint16_t pixels[4];
} texture_blob_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, light_levels;
    uint32_t palette_entries, index_bytes;
    uint16_t light_lut[16 * 256];
    uint8_t indices[4];
} wall_blob_t;

static texture_blob_t s_texture = {
    .magic = ATLAS_MAGIC,
    .width = 2,
    .height = 2,
    .rgb_bytes = 8,
    .pixels = {1, 2, 3, 4},
};
static uint8_t s_invalid[4] = {0};
static wall_blob_t s_wall = {
    .magic = WALL_MAGIC,
    .width = 2,
    .height = 2,
    .light_levels = 16,
    .palette_entries = 256,
    .index_bytes = 4,
    .indices = {0, 1, 2, 3},
};
static unsigned s_open_count;
static unsigned s_release_count;

esp_err_t mosaico_game_asset_open(const char *name, mosaico_asset_view_t *out)
{
    if (!name || !out) return ESP_ERR_INVALID_ARG;
    ++s_open_count;
    if (strcmp(name, "texture") == 0) {
        *out = (mosaico_asset_view_t) {
            .name = name, .data = (const uint8_t *)&s_texture,
            .size = sizeof(s_texture),
        };
        return ESP_OK;
    }
    if (strcmp(name, "wall") == 0) {
        *out = (mosaico_asset_view_t) {
            .name = name, .data = (const uint8_t *)&s_wall,
            .size = sizeof(s_wall),
        };
        return ESP_OK;
    }
    if (strcmp(name, "invalid") == 0) {
        *out = (mosaico_asset_view_t) {
            .name = name, .data = s_invalid, .size = sizeof(s_invalid),
        };
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

void mosaico_game_asset_release(mosaico_asset_view_t *view)
{
    if (!view || !view->data) return;
    ++s_release_count;
    view->data = NULL;
    view->size = 0;
}

int main(void)
{
    raylib_lite_renderer_texture_t invalid =
        raylib_lite_renderer_load_texture("invalid");
    assert(invalid.id == 0);
    assert(s_open_count == 1 && s_release_count == 1);

    raylib_lite_renderer_texture_t texture =
        raylib_lite_renderer_load_texture("texture");
    assert(texture.id && texture.width == 2 && texture.height == 2);
    assert(s_open_count == 2 && s_release_count == 1);
    raylib_lite_renderer_unload_texture(texture);
    assert(s_release_count == 2);
    raylib_lite_renderer_unload_texture(texture);
    assert(s_release_count == 2);

    uint16_t pixels[4] = {5, 6, 7, 8};
    raylib_lite_renderer_texture_t registered =
        raylib_lite_renderer_register_rgb565(pixels, 2, 2);
    assert(registered.id);
    raylib_lite_renderer_unload_texture(registered);
    assert(s_release_count == 2);

    raylib_lite_wall_atlas_t walls[20] = {0};
    for (size_t i = 0; i < 20; ++i) {
        walls[i] = raylib_lite_wall_atlas_load("wall");
        assert(walls[i].descriptor == &s_wall);
    }
    assert(s_open_count == 22 && s_release_count == 2);

    for (size_t i = 0; i < 20; ++i)
        raylib_lite_wall_atlas_unload(walls[i]);
    assert(s_release_count == 22);

    puts("renderer core: ok");
    return 0;
}
