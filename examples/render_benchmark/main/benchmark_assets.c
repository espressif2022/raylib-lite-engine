// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_assets.h"
#include <string.h>
/* Test data is generated in RAM. Only the stack suite's tilemap asks for
 * these two blobs; every other name fails explicitly. */
#define BENCH_ATLAS_MAGIC 0x3141534dU
#define BENCH_MAP_MAGIC 0x314d544dU
#define BENCH_ATLAS_ID 0xA71A5001U
#define BENCH_TILE 16
#define BENCH_TILES 4
#define BENCH_MAP_W 8
#define BENCH_MAP_H 6

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, flags;
    uint32_t rgb_bytes, alpha_bytes;
} bench_atlas_header_t;
typedef struct __attribute__((packed)) {
    bench_atlas_header_t header;
    uint16_t rgb[BENCH_TILE * (BENCH_TILES * BENCH_TILE)];
} bench_atlas_t;
typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, tile_width, tile_height, layers, objects;
    uint32_t points, layer_bytes, collision_bytes, atlas_id;
} bench_map_header_t;
typedef struct __attribute__((packed)) {
    bench_map_header_t header;
    uint16_t tiles[BENCH_MAP_W * BENCH_MAP_H];
    uint8_t collision[(BENCH_MAP_W * BENCH_MAP_H + 7) / 8];
} bench_map_t;

static bench_atlas_t s_atlas;
static bench_map_t s_map;
static bool s_ready;

static void ensure(void)
{
    if (s_ready) return;
    s_atlas.header = (bench_atlas_header_t) {
        .magic = BENCH_ATLAS_MAGIC,
        .width = BENCH_TILES * BENCH_TILE, .height = BENCH_TILE,
        .rgb_bytes = sizeof(s_atlas.rgb),
    };
    for (int tile = 0; tile < BENCH_TILES; ++tile)
        for (int y = 0; y < BENCH_TILE; ++y)
            for (int x = 0; x < BENCH_TILE; ++x)
                s_atlas.rgb[y * s_atlas.header.width + tile * BENCH_TILE + x] =
                    (uint16_t)((tile * 0x2940) ^ (x * 37 + y * 101));
    s_map.header = (bench_map_header_t) {
        .magic = BENCH_MAP_MAGIC,
        .width = BENCH_MAP_W, .height = BENCH_MAP_H,
        .tile_width = BENCH_TILE, .tile_height = BENCH_TILE, .layers = 1,
        .layer_bytes = sizeof(s_map.tiles),
        .collision_bytes = sizeof(s_map.collision),
        .atlas_id = BENCH_ATLAS_ID,
    };
    for (int i = 0; i < BENCH_MAP_W * BENCH_MAP_H; ++i)
        s_map.tiles[i] = (uint16_t)((i * 3) % (BENCH_TILES + 1));
    s_map.collision[0] = 0x2;
    s_ready = true;
}

raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    ensure();
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (name && strcmp(name, "bench-atlas") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .id = BENCH_ATLAS_ID, .name = "bench-atlas",
            .data = (const uint8_t *)&s_atlas, .size = sizeof(s_atlas),
        };
        return RAYLIB_LITE_OK;
    }
    if (name && strcmp(name, "bench-map") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .id = 1, .name = "bench-map",
            .data = (const uint8_t *)&s_map, .size = sizeof(s_map),
        };
        return RAYLIB_LITE_OK;
    }
    return RAYLIB_LITE_NOT_SUPPORTED;
}

raylib_lite_result_t raylib_lite_asset_open_id(raylib_lite_asset_id_t id,
                                     raylib_lite_asset_view_t *out)
{
    ensure();
    if (!out || id != BENCH_ATLAS_ID) return RAYLIB_LITE_NOT_FOUND;
    *out = (raylib_lite_asset_view_t) {
        .id = id, .name = "bench-atlas",
        .data = (const uint8_t *)&s_atlas, .size = sizeof(s_atlas),
    };
    return RAYLIB_LITE_OK;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{ if (view) { view->data = NULL; view->size = 0; } }
