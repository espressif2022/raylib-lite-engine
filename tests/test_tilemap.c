// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

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
static int s_row_x[128], s_row_y[128];
static size_t s_row_count[128];
static const uint8_t *s_asset_data;
static size_t s_asset_size;
#define CANVAS 80
static uint16_t s_pixels[CANVAS][CANVAS];

static void paint(int x, int y, int w, int h, uint16_t id)
{
    for (int py = y; py < y + h; ++py) {
        if (py < 0 || py >= CANVAS) continue;
        for (int px = x; px < x + w; ++px) {
            if (px >= 0 && px < CANVAS) s_pixels[py][px] = id;
        }
    }
}

static void reset_blob(void)
{
    memset(&s_blob, 0, sizeof(s_blob));
    s_asset_data = (const uint8_t *)&s_blob;
    s_asset_size = sizeof(s_blob);
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
        .data = s_asset_data,
        .size = s_asset_size,
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
    assert(count > 0 && count <= 64);
    assert(tile_width == 16 && tile_height == 16);
    assert(s_rows < 128);
    s_row_count[s_rows] = count;
    for (size_t i = 0; i < count; ++i) {
        if (tiles[i]) paint(x + (int)i * tile_width, y, tile_width, tile_height, tiles[i]);
    }
    s_row_x[s_rows] = x;
    s_row_y[s_rows] = y;
    ++s_rows;
}

static unsigned s_quads;
static raylib_lite_renderer_rect_t s_quad_src[256], s_quad_dst[256];

void raylib_lite_renderer_draw_texture_pro(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, raylib_lite_renderer_rect_t dest,
    raylib_lite_renderer_vec2_t origin, float rotation,
    raylib_lite_renderer_color_t tint)
{
    (void)origin;
    (void)rotation;
    (void)tint;
    assert(texture.id == 1);
    assert(s_quads < 256);
    paint((int)dest.x, (int)dest.y, (int)dest.width, (int)dest.height,
          (uint16_t)(source.x / source.width) + 1);
    s_quad_src[s_quads] = source;
    s_quad_dst[s_quads] = dest;
    ++s_quads;
}

static void assert_rejected(void)
{
    unsigned before = s_release_count;
    assert(raylib_lite_tilemap_load("map") == NULL);
    assert(s_release_count == before + 1);
}

static void clear_draw(void)
{
    s_rows = s_quads = 0;
    memset(s_pixels, 0, sizeof(s_pixels));
}

static void test_empty_and_partial_viewports(raylib_lite_tilemap_t map)
{
    const raylib_lite_renderer_rect_t empty[] = {
        {32, 0, 16, 16}, {0, 32, 16, 16}, {-16, 0, 16, 16},
        {0, -16, 16, 16}, {0, 0, 0, 16}, {0, 0, 16, 0},
        {0, 0, -1, 16}, {0, 0, 16, -1}, {NAN, 0, 16, 16},
        {0, 0, INFINITY, 16}, {FLT_MAX, FLT_MAX, 16, 16},
    };
    for (size_t i = 0; i < sizeof(empty) / sizeof(empty[0]); ++i) {
        clear_draw();
        raylib_lite_tilemap_draw_layer(map, 0, empty[i],
                                      (raylib_lite_renderer_vec2_t){0, 0});
        raylib_lite_tilemap_draw_layer_scaled(map, 0, empty[i],
                                      (raylib_lite_renderer_vec2_t){0, 0}, 1.002f);
        assert(s_rows == 0 && s_quads == 0);
    }
    clear_draw();
    raylib_lite_tilemap_draw_layer(map, 0,
        (raylib_lite_renderer_rect_t){31.9f, -10, .2f, 10.1f},
        (raylib_lite_renderer_vec2_t){3, 7});
    assert(s_rows == 1 && s_row_count[0] == 1);
    assert(s_row_x[0] == 19 && s_row_y[0] == 7);
}

static void test_scale_oracle(raylib_lite_tilemap_t map)
{
    const float scales[] = {.5f, .999f, 1.0f, 1.0005f, 1.002f, 1.5f};
    const raylib_lite_renderer_rect_t viewports[] = {
        {0, 0, 32, 32}, {16.25f, 0, 15.75f, 32},
        {-2.5f, -1.5f, 18.6f, 18}, {15.9f, 16.1f, .2f, 15.9f},
    };
    const raylib_lite_renderer_vec2_t origins[] = {{3, 7}, {-3.25f, 8.75f}};
    for (size_t si = 0; si < sizeof(scales) / sizeof(scales[0]); ++si) {
        float scale = scales[si];
        for (size_t vi = 0; vi < sizeof(viewports) / sizeof(viewports[0]); ++vi) {
            raylib_lite_renderer_rect_t v = viewports[vi];
            for (size_t oi = 0; oi < sizeof(origins) / sizeof(origins[0]); ++oi) {
                raylib_lite_renderer_vec2_t origin = origins[oi];
                clear_draw();
                raylib_lite_tilemap_draw_layer_scaled(map, 0, v, origin, scale);
                /* Inverse pixel lookup is independent of draw-row / quad placement:
                 * the right edge of a pixel identifies its snapped tile cell. */
                for (int y = 0; y < CANVAS; ++y) {
                    for (int x = 0; x < CANVAS; ++x) {
                        int tx = (int)ceil((x + 1.0 - origin.x) / (16.0 * scale)) - 1;
                        int ty = (int)ceil((y + 1.0 - origin.y) / (16.0 * scale)) - 1;
                        uint16_t expected = 0;
                        if (tx >= 0 && tx < 2 && ty >= 0 && ty < 2 &&
                                (tx + 1) * 16 > v.x && tx * 16 < (double)v.x + v.width &&
                                (ty + 1) * 16 > v.y && ty * 16 < (double)v.y + v.height)
                            expected = (uint16_t)(ty * 2 + tx + 1);
                        assert(s_pixels[y][x] == expected);
                    }
                }
            }
        }
    }
}

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    put_u16(p, (uint16_t)v);
    put_u16(p + 2, (uint16_t)(v >> 16));
}

static void test_row_chunks_and_layers(void)
{
    _Alignas(uint32_t) uint8_t data[601] = {0};
    put_u32(data, MTM_MAGIC);
    put_u16(data + 4, 70);
    put_u16(data + 6, 1);
    put_u16(data + 8, 16);
    put_u16(data + 10, 16);
    put_u16(data + 12, 2);
    put_u32(data + 20, 280);
    put_u32(data + 24, 9);
    put_u32(data + 28, 0x12345678U);
    for (unsigned x = 0; x < 70; ++x) {
        put_u16(data + 32 + x * 2, 1);
        put_u16(data + 172 + x * 2, (uint16_t)(x + 1));
    }
    s_asset_data = data;
    s_asset_size = 321;
    raylib_lite_tilemap_t map = raylib_lite_tilemap_load("map");
    assert(map);
    clear_draw();
    raylib_lite_tilemap_draw_layer(map, 1,
        (raylib_lite_renderer_rect_t){0, 0, 1120, 16},
        (raylib_lite_renderer_vec2_t){0, 0});
    assert(s_rows == 2 && s_row_count[0] == 64 && s_row_count[1] == 6);
    assert(s_row_x[0] == 0 && s_row_x[1] == 1024);
    clear_draw();
    raylib_lite_tilemap_draw_layer(map, 1,
        (raylib_lite_renderer_rect_t){960, 0, 160, 16},
        (raylib_lite_renderer_vec2_t){-960, 0});
    assert(s_rows == 1 && s_row_count[0] == 10);
    assert(s_pixels[0][0] == 61 && s_pixels[0][16] == 62);
    raylib_lite_tilemap_unload(map);
}

static void test_binary_alignment(void)
{
    /* Explicit bytes check format endianness as well as odd-address backing,
     * odd/even collision lengths, object offsets and signed path coordinates. */
    _Alignas(uint32_t) uint8_t storage[256];
    for (unsigned offset = 0; offset <= 1; ++offset) {
        for (unsigned width = 2; width <= 9; width += 7) {
            uint8_t *data = storage + offset;
            memset(storage, 0, sizeof(storage));
            unsigned collision = (width + 7) / 8;
            put_u32(data, MTM_MAGIC);
            put_u16(data + 4, (uint16_t)width);
            put_u16(data + 6, 1);
            put_u16(data + 8, 16);
            put_u16(data + 10, 16);
            put_u16(data + 12, 1);
            put_u16(data + 14, 1);
            put_u32(data + 16, 2);
            put_u32(data + 20, width * 2);
            put_u32(data + 24, collision);
            put_u32(data + 28, 0x12345678U);
            for (unsigned x = 0; x < width; ++x) put_u16(data + 32 + x * 2, 0x0102);
            uint8_t *object = data + 32 + width * 2 + collision;
            data[32 + width * 2] = 2;
            put_u32(object, 0x87654321U);
            put_u16(object + 4, (uint16_t)-123);
            put_u16(object + 6, 456);
            put_u16(object + 8, 17);
            put_u16(object + 10, 29);
            put_u32(object + 12, 0xaabbccddU);
            put_u16(object + 16, (uint16_t)-32768);
            put_u16(object + 18, 32767);
            put_u16(object + 20, 513);
            put_u16(object + 22, (uint16_t)-257);
            s_asset_data = data;
            s_asset_size = (size_t)(object + 24 - data);
            raylib_lite_tilemap_t map = raylib_lite_tilemap_load("map");
            assert(map);
            assert(raylib_lite_tilemap_is_blocked(map, 1, 0));
            raylib_lite_map_object_t out;
            assert(raylib_lite_tilemap_find_object(map, 0x87654321U, &out));
            assert(out.x == -123 && out.y == 456 && out.width == 17 && out.height == 29);
            assert(out.flags == 0xaabbccddU);
            assert(raylib_lite_tilemap_object_at(map, 0, &out));
            assert(!raylib_lite_tilemap_object_at(map, 1, &out));
            const raylib_lite_renderer_vec2_t *points;
            assert(raylib_lite_tilemap_path_points(map, &points) == 2);
            assert(points[0].x == -32768 && points[0].y == 32767);
            assert(points[1].x == 513 && points[1].y == -257);
            clear_draw();
            raylib_lite_tilemap_draw_layer(map, 0,
                (raylib_lite_renderer_rect_t){0, 0, 32, 16},
                (raylib_lite_renderer_vec2_t){0, 0});
            assert(s_pixels[0][0] == 0x0102 && s_pixels[0][16] == 0x0102);
            raylib_lite_tilemap_unload(map);
        }
    }
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

    s_rows = 0;
    raylib_lite_tilemap_draw_layer_scaled(
        map, 0, (raylib_lite_renderer_rect_t){0, 0, 32, 32},
        (raylib_lite_renderer_vec2_t){3, 7}, 1.0f);
    assert(s_rows == 2 && s_quads == 0);

    raylib_lite_tilemap_draw_layer_scaled(
        map, 0, (raylib_lite_renderer_rect_t){0, 0, 32, 32},
        (raylib_lite_renderer_vec2_t){3, 7}, 0.5f);
    assert(s_quads == 4);
    assert(s_quad_dst[0].x == 3 && s_quad_dst[0].y == 7);
    assert(s_quad_dst[0].width == 8 && s_quad_dst[0].height == 8);
    assert(s_quad_dst[3].x == 11 && s_quad_dst[3].y == 15);
    assert(s_quad_src[3].x == 48 && s_quad_src[3].width == 16);

    test_empty_and_partial_viewports(map);
    test_scale_oracle(map);
    raylib_lite_tilemap_unload(map);
    test_binary_alignment();
    test_row_chunks_and_layers();
    puts("tilemap: ok");
    return 0;
}
