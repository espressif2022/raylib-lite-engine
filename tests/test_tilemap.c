// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib_lite_tilemap.h"

#define MTM_MAGIC 0x314d544dU

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, tile_width, tile_height, layers, objects;
    uint32_t points, layer_bytes, collision_bytes, atlas_id;
} test_map_header_t;

typedef struct {
    test_map_header_t header;
    uint16_t tiles[4];
    uint8_t collision;
} test_map_blob_t;

static test_map_blob_t s_blob;
static unsigned s_release_count;
static unsigned s_rows;
static int s_row_x[2], s_row_y[2];

static void reset_blob(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_blob.header = (test_map_header_t) {
        .magic = MTM_MAGIC,
        .width = 2,
        .height = 2,
        .tile_width = 16,
        .tile_height = 16,
        .layers = 1,
        .layer_bytes = sizeof(s_blob.tiles),
        .collision_bytes = sizeof(s_blob.collision),
        .atlas_id = 0x12345678U,
    };
    s_blob.tiles[0] = 1;
    s_blob.tiles[1] = 2;
    s_blob.tiles[2] = 3;
    s_blob.tiles[3] = 4;
    s_blob.collision = 1U << 1;
}

raylib_lite_result_t raylib_lite_asset_open(
    const char *name, raylib_lite_asset_view_t *out)
{
    if (!name || !out || strcmp(name, "map") != 0)
        return RAYLIB_LITE_NOT_FOUND;
    *out = (raylib_lite_asset_view_t) {
        .id = 1,
        .name = name,
        .data = (const uint8_t *)&s_blob,
        .size = sizeof(s_blob),
    };
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_asset_open_id(
    raylib_lite_asset_id_t id, raylib_lite_asset_view_t *out)
{
    if (!out || id != s_blob.header.atlas_id) return RAYLIB_LITE_NOT_FOUND;
    *out = (raylib_lite_asset_view_t) {
        .id = id,
        .name = "atlas",
        .data = (const uint8_t *)"x",
        .size = 1,
    };
    return RAYLIB_LITE_OK;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (!view || !view->data) return;
    ++s_release_count;
    view->data = NULL;
}

raylib_lite_renderer_atlas_t raylib_lite_renderer_load_atlas(const char *path)
{
    assert(path && strcmp(path, "atlas") == 0);
    return (raylib_lite_renderer_atlas_t) {
        .texture = {.id = 1, .width = 32, .height = 32},
    };
}

void raylib_lite_renderer_unload_atlas(raylib_lite_renderer_atlas_t atlas)
{
    (void)atlas;
}

void raylib_lite_renderer_draw_tile_row(
    raylib_lite_renderer_texture_t texture, const uint16_t *tiles,
    size_t count, int tile_width, int tile_height, int x, int y)
{
    assert(texture.id == 1);
    assert(tiles != NULL);
    assert(count == 2);
    assert(tile_width == 16 && tile_height == 16);
    assert(s_rows < 2);
    s_row_x[s_rows] = x;
    s_row_y[s_rows] = y;
    ++s_rows;
}

static void assert_rejected(void)
{
    unsigned before = s_release_count;
    assert(raylib_lite_tilemap_load("map") == NULL);
    assert(s_release_count == before + 1);
}

int main(void)
{
    reset_blob();
    s_blob.header.tile_width = 0;
    assert_rejected();

    reset_blob();
    s_blob.header.layer_bytes -= sizeof(uint16_t);
    assert_rejected();

    reset_blob();
    s_blob.header.collision_bytes = 0;
    assert_rejected();

    reset_blob();
    raylib_lite_tilemap_t map = raylib_lite_tilemap_load("map");
    assert(map != NULL);
    assert(raylib_lite_tilemap_is_blocked(map, 1, 0));
    assert(!raylib_lite_tilemap_is_blocked(map, 0, 0));

    s_rows = 0;
    raylib_lite_tilemap_draw_layer(
        map, 0, (raylib_lite_renderer_rect_t){0, 0, 32, 32},
        (raylib_lite_renderer_vec2_t){3, 7});
    assert(s_rows == 2);
    assert(s_row_x[0] == 3 && s_row_x[1] == 3);
    assert(s_row_y[0] == 7 && s_row_y[1] == 23);

    raylib_lite_tilemap_unload(map);
    puts("tilemap: ok");
    return 0;
}
