// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_tilemap.h"

#include <string.h>

#define MTM_MAGIC 0x314d544dU
#define MTM_SLOTS 4

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, tile_width, tile_height, layers, objects;
    uint32_t points, layer_bytes, collision_bytes, atlas_id;
} map_header_t;
typedef struct __attribute__((packed)) {
    uint32_t id;
    int16_t x, y, width, height;
    uint32_t flags;
} map_object_t;

struct raylib_lite_tilemap_slot {
    bool used;
    raylib_lite_asset_view_t asset;
    const map_header_t *header;
    const uint16_t *layers;
    const uint8_t *collision;
    const map_object_t *objects;
    const int16_t *points;
    raylib_lite_renderer_atlas_t atlas;
    raylib_lite_renderer_vec2_t point_cache[32];
};
static struct raylib_lite_tilemap_slot s_maps[MTM_SLOTS];

raylib_lite_tilemap_t raylib_lite_tilemap_load(const char *path)
{
    raylib_lite_asset_view_t asset = {0};
    if (raylib_lite_asset_open(path, &asset) != RAYLIB_LITE_OK ||
            asset.size < sizeof(map_header_t)) {
        raylib_lite_asset_release(&asset);
        return NULL;
    }
    const map_header_t *header = (const map_header_t *)asset.data;
    size_t need = sizeof(*header) + header->layer_bytes +
                  header->collision_bytes +
                  (size_t)header->objects * sizeof(map_object_t) +
                  (size_t)header->points * 4U;
    if (header->magic != MTM_MAGIC || need > asset.size || header->points > 32) {
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
        map->header = header;
        map->layers = (const uint16_t *)(asset.data + sizeof(*header));
        map->collision = asset.data + sizeof(*header) + header->layer_bytes;
        map->objects = (const map_object_t *)(map->collision +
                                             header->collision_bytes);
        map->points = (const int16_t *)(map->objects + header->objects);
        map->atlas = atlas;
        for (uint32_t point = 0; point < header->points; ++point) {
            map->point_cache[point] = (raylib_lite_renderer_vec2_t) {
                map->points[point * 2], map->points[point * 2 + 1],
            };
        }
        return map;
    }
    raylib_lite_renderer_unload_atlas(atlas);
    raylib_lite_asset_release(&asset);
    return NULL;
}

void raylib_lite_tilemap_draw_layer(raylib_lite_tilemap_t map,
    uint32_t layer, raylib_lite_renderer_rect_t viewport)
{
    if (!map || !map->used || layer >= map->header->layers) return;
    int x0 = (int)viewport.x / map->header->tile_width;
    int y0 = (int)viewport.y / map->header->tile_height;
    int x1 = (int)(viewport.x + viewport.width + map->header->tile_width - 1) /
             map->header->tile_width;
    int y1 = (int)(viewport.y + viewport.height + map->header->tile_height - 1) /
             map->header->tile_height;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > map->header->width) x1 = map->header->width;
    if (y1 > map->header->height) y1 = map->header->height;
    const uint16_t *tiles = map->layers +
        (size_t)layer * map->header->width * map->header->height;
    for (int y = y0; y < y1; ++y) {
        raylib_lite_renderer_draw_tile_row(map->atlas.texture,
            &tiles[(size_t)y * map->header->width + x0],
            (size_t)(x1 - x0), map->header->tile_width,
            map->header->tile_height, x0 * map->header->tile_width,
            69 + y * map->header->tile_height);
    }
}

bool raylib_lite_tilemap_is_blocked(raylib_lite_tilemap_t map, int x, int y)
{
    if (!map || x < 0 || y < 0 || x >= map->header->width ||
            y >= map->header->height) return true;
    size_t i = (size_t)y * map->header->width + x;
    return (map->collision[i / 8] & (1U << (i & 7))) != 0;
}

static void copy_object(const map_object_t *in, raylib_lite_map_object_t *out)
{
    *out = (raylib_lite_map_object_t) {
        in->id, in->x, in->y, in->width, in->height, in->flags,
    };
}

bool raylib_lite_tilemap_find_object(raylib_lite_tilemap_t map,
    raylib_lite_asset_id_t id, raylib_lite_map_object_t *out)
{
    if (!map || !out) return false;
    for (uint16_t i = 0; i < map->header->objects; ++i) {
        if (map->objects[i].id == id) {
            copy_object(&map->objects[i], out);
            return true;
        }
    }
    return false;
}

bool raylib_lite_tilemap_object_at(raylib_lite_tilemap_t map, size_t index,
                                   raylib_lite_map_object_t *out)
{
    if (!map || !out || index >= map->header->objects) return false;
    copy_object(&map->objects[index], out);
    return true;
}

size_t raylib_lite_tilemap_path_points(raylib_lite_tilemap_t map,
    const raylib_lite_renderer_vec2_t **out_points)
{
    if (!map || !out_points) return 0;
    *out_points = map->point_cache;
    return map->header->points;
}

void raylib_lite_tilemap_unload(raylib_lite_tilemap_t map)
{
    if (!map || !map->used) return;
    raylib_lite_renderer_unload_atlas(map->atlas);
    raylib_lite_asset_release(&map->asset);
    memset(map, 0, sizeof(*map));
}
