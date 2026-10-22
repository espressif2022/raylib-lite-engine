// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_tilemap.h"

#include <string.h>
#include <limits.h>
#include <math.h>

#define MTM_MAGIC 0x314d544dU
#define MTM_SLOTS 4

typedef struct {
    uint32_t magic;
    uint16_t width, height, tile_width, tile_height, layers, objects;
    uint32_t points, layer_bytes, collision_bytes, atlas_id;
} map_header_t;
/* MTM is little-endian and has no padding, including after collision bits. */
#define MAP_HEADER_BYTES 32U
#define MAP_OBJECT_BYTES 16U
#define TILE_ROW_CHUNK 64U

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)p[1] << 8);
}

static int16_t read_i16(const uint8_t *p)
{
    uint16_t value = read_u16(p);
    return (int16_t)(value < 0x8000U ? (int32_t)value : (int32_t)value - 0x10000);
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)read_u16(p) | (uint32_t)read_u16(p + 2) << 16;
}

static map_header_t read_header(const uint8_t *p)
{
    return (map_header_t) {
        read_u32(p), read_u16(p + 4), read_u16(p + 6),
        read_u16(p + 8), read_u16(p + 10), read_u16(p + 12),
        read_u16(p + 14), read_u32(p + 16), read_u32(p + 20),
        read_u32(p + 24), read_u32(p + 28),
    };
}

struct raylib_lite_tilemap_slot {
    bool used;
    raylib_lite_asset_view_t asset;
    map_header_t header;
    const uint8_t *layers;
    const uint8_t *collision;
    const uint8_t *objects;
    raylib_lite_renderer_atlas_t atlas;
    raylib_lite_renderer_vec2_t point_cache[32];
};
static struct raylib_lite_tilemap_slot s_maps[MTM_SLOTS];

raylib_lite_tilemap_t raylib_lite_tilemap_load(const char *path)
{
    raylib_lite_asset_view_t asset = {0};
    if (raylib_lite_asset_open(path, &asset) != RAYLIB_LITE_OK ||
            !asset.data || asset.size < MAP_HEADER_BYTES) {
        raylib_lite_asset_release(&asset);
        return NULL;
    }
    const map_header_t decoded = read_header(asset.data);
    const map_header_t *header = &decoded;
    uint64_t cells = (uint64_t)header->width * header->height;
    uint64_t expected_layer_bytes =
        cells * header->layers * sizeof(uint16_t);
    uint64_t expected_collision_bytes = (cells + 7U) / 8U;
    uint64_t need = MAP_HEADER_BYTES + expected_layer_bytes +
                    expected_collision_bytes +
                    (uint64_t)header->objects * MAP_OBJECT_BYTES +
                    (uint64_t)header->points * 4U;
    if (header->magic != MTM_MAGIC || !header->width || !header->height ||
            !header->tile_width || !header->tile_height || !header->layers ||
            header->points > 32 ||
            header->layer_bytes != expected_layer_bytes ||
            header->collision_bytes != expected_collision_bytes ||
            need > asset.size) {
        raylib_lite_asset_release(&asset);
        return NULL;
    }

    raylib_lite_asset_view_t atlas_view = {0};
    if (raylib_lite_asset_open_id(header->atlas_id, &atlas_view) != RAYLIB_LITE_OK) {
        raylib_lite_asset_release(&asset);
        return NULL;
    }
    raylib_lite_renderer_atlas_t atlas =
        raylib_lite_renderer_load_atlas(atlas_view.name);
    raylib_lite_asset_release(&atlas_view);
    if (!atlas.texture.id) {
        raylib_lite_asset_release(&asset);
        return NULL;
    }

    for (unsigned i = 0; i < MTM_SLOTS; ++i) {
        if (s_maps[i].used) continue;
        struct raylib_lite_tilemap_slot *map = &s_maps[i];
        memset(map, 0, sizeof(*map));
        map->used = true;
        map->asset = asset;
        map->header = *header;
        map->layers = asset.data + MAP_HEADER_BYTES;
        map->collision = asset.data + MAP_HEADER_BYTES + header->layer_bytes;
        map->objects = map->collision + header->collision_bytes;
        const uint8_t *points = map->objects +
                                (size_t)header->objects * MAP_OBJECT_BYTES;
        map->atlas = atlas;
        for (uint32_t point = 0; point < header->points; ++point) {
            map->point_cache[point] = (raylib_lite_renderer_vec2_t) {
                read_i16(points + point * 4), read_i16(points + point * 4 + 2),
            };
        }
        return map;
    }
    raylib_lite_renderer_unload_atlas(atlas);
    raylib_lite_asset_release(&asset);
    return NULL;
}

/* Intersect in map pixels before converting to tile indices or forming pointers.
 * Double intermediates also avoid overflow for large finite float viewports. */
static bool visible_tiles(raylib_lite_tilemap_t map, uint32_t layer,
    raylib_lite_renderer_rect_t viewport, int *x0, int *y0, int *x1, int *y1)
{
    if (!map || !map->used || layer >= map->header.layers ||
            !isfinite(viewport.x) || !isfinite(viewport.y) ||
            !isfinite(viewport.width) || !isfinite(viewport.height) ||
            viewport.width <= 0 || viewport.height <= 0) return false;
    const double tw = map->header.tile_width, th = map->header.tile_height;
    double left = viewport.x, top = viewport.y;
    double right = left + (double)viewport.width;
    double bottom = top + (double)viewport.height;
    const double width = tw * map->header.width;
    const double height = th * map->header.height;
    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > width) right = width;
    if (bottom > height) bottom = height;
    if (left >= right || top >= bottom) return false;
    *x0 = (int)(left / tw);
    *y0 = (int)(top / th);
    *x1 = (int)(right / tw);
    *y1 = (int)(bottom / th);
    if (*x1 * tw < right) ++*x1;
    if (*y1 * th < bottom) ++*y1;
    return true;
}

static bool valid_screen_bounds(raylib_lite_tilemap_t map,
    raylib_lite_renderer_vec2_t origin, float scale, int x0, int y0, int x1, int y1)
{
    if (!isfinite(origin.x) || !isfinite(origin.y) ||
            !isfinite(scale) || scale <= 0) return false;
    return (double)origin.x + (double)x0 * map->header.tile_width * scale >= INT_MIN &&
           (double)origin.y + (double)y0 * map->header.tile_height * scale >= INT_MIN &&
           (double)origin.x + (double)x1 * map->header.tile_width * scale <= INT_MAX &&
           (double)origin.y + (double)y1 * map->header.tile_height * scale <= INT_MAX;
}

static int floor_px(double v)
{
    int i = (int)v;
    return i - (v < (double)i);
}

void raylib_lite_tilemap_draw_layer(raylib_lite_tilemap_t map,
    uint32_t layer, raylib_lite_renderer_rect_t viewport,
    raylib_lite_renderer_vec2_t origin)
{
    int x0, y0, x1, y1;
    if (!visible_tiles(map, layer, viewport, &x0, &y0, &x1, &y1) ||
            !valid_screen_bounds(map, origin, 1, x0, y0, x1, y1)) return;
    const uint8_t *tiles = map->layers +
        (size_t)layer * map->header.width * map->header.height * 2U;
    /* A small aligned scratch row works with odd-address asset backing and on
     * big-endian hosts, without allocating another copy of the entire map. */
    uint16_t row[TILE_ROW_CHUNK];
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1;) {
            size_t count = (size_t)(x1 - x);
            if (count > TILE_ROW_CHUNK) count = TILE_ROW_CHUNK;
            const uint8_t *source = tiles + ((size_t)y * map->header.width + x) * 2U;
            for (size_t i = 0; i < count; ++i) row[i] = read_u16(source + i * 2U);
            raylib_lite_renderer_draw_tile_row(map->atlas.texture,
                row, count, map->header.tile_width, map->header.tile_height,
                floor_px((double)origin.x + (double)x * map->header.tile_width),
                floor_px((double)origin.y + (double)y * map->header.tile_height));
            x += (int)count;
        }
    }
}

void raylib_lite_tilemap_draw_layer_scaled(raylib_lite_tilemap_t map,
    uint32_t layer, raylib_lite_renderer_rect_t viewport,
    raylib_lite_renderer_vec2_t origin, float scale)
{
    if (scale == 1.0f) {
        raylib_lite_tilemap_draw_layer(map, layer, viewport, origin);
        return;
    }
    int x0, y0, x1, y1;
    if (!visible_tiles(map, layer, viewport, &x0, &y0, &x1, &y1) ||
            !valid_screen_bounds(map, origin, scale, x0, y0, x1, y1)) return;
    const int tw = map->header.tile_width, th = map->header.tile_height;
    const uint8_t *tiles = map->layers +
        (size_t)layer * map->header.width * map->header.height * 2U;
    for (int y = y0; y < y1; ++y) {
        /* Snap shared edges; origin always denotes map (0,0), not the first
         * visible tile. Fractional viewport movement changes only culling. */
        float top = (float)floor_px((double)origin.y + (double)y * th * scale);
        float bottom = (float)floor_px((double)origin.y + (double)(y + 1) * th * scale);
        for (int x = x0; x < x1; ++x) {
            uint16_t id = read_u16(tiles + ((size_t)y * map->header.width + x) * 2U);
            if (!id) continue;
            float left = (float)floor_px((double)origin.x + (double)x * tw * scale);
            float right = (float)floor_px((double)origin.x + (double)(x + 1) * tw * scale);
            raylib_lite_renderer_draw_texture_pro(map->atlas.texture,
                (raylib_lite_renderer_rect_t){(float)((id - 1U) * tw), 0, (float)tw, (float)th},
                (raylib_lite_renderer_rect_t){left, top, right - left, bottom - top},
                (raylib_lite_renderer_vec2_t){0, 0}, 0,
                (raylib_lite_renderer_color_t){255, 255, 255, 255});
        }
    }
}

bool raylib_lite_tilemap_is_blocked(raylib_lite_tilemap_t map, int x, int y)
{
    if (!map || !map->used || x < 0 || y < 0 || x >= map->header.width ||
            y >= map->header.height) return true;
    size_t i = (size_t)y * map->header.width + x;
    return (map->collision[i / 8] & (1U << (i & 7))) != 0;
}

static void copy_object(const uint8_t *in, raylib_lite_map_object_t *out)
{
    *out = (raylib_lite_map_object_t) {
        read_u32(in), read_i16(in + 4), read_i16(in + 6),
        read_i16(in + 8), read_i16(in + 10), read_u32(in + 12),
    };
}

bool raylib_lite_tilemap_find_object(raylib_lite_tilemap_t map,
    raylib_lite_asset_id_t id, raylib_lite_map_object_t *out)
{
    if (!map || !map->used || !out) return false;
    for (uint16_t i = 0; i < map->header.objects; ++i) {
        const uint8_t *object = map->objects + (size_t)i * MAP_OBJECT_BYTES;
        if (read_u32(object) == id) {
            copy_object(object, out);
            return true;
        }
    }
    return false;
}

bool raylib_lite_tilemap_object_at(raylib_lite_tilemap_t map, size_t index,
                                   raylib_lite_map_object_t *out)
{
    if (!map || !map->used || !out || index >= map->header.objects) return false;
    copy_object(map->objects + index * MAP_OBJECT_BYTES, out);
    return true;
}

size_t raylib_lite_tilemap_path_points(raylib_lite_tilemap_t map,
    const raylib_lite_renderer_vec2_t **out_points)
{
    if (!map || !map->used || !out_points) return 0;
    *out_points = map->point_cache;
    return map->header.points;
}

void raylib_lite_tilemap_unload(raylib_lite_tilemap_t map)
{
    if (!map || !map->used) return;
    raylib_lite_renderer_unload_atlas(map->atlas);
    raylib_lite_asset_release(&map->asset);
    memset(map, 0, sizeof(*map));
}
