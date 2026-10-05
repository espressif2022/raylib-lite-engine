// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mosaico_game_assets.h"
#include "raylib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct raylib_lite_tilemap_slot *raylib_lite_tilemap_t;
typedef struct {mosaico_asset_id_t id;int16_t x,y,width,height;uint32_t flags;} raylib_lite_map_object_t;
raylib_lite_tilemap_t raylib_lite_tilemap_load(const char *asset_path);
void raylib_lite_tilemap_draw_layer(raylib_lite_tilemap_t map,uint32_t layer_id,Rectangle viewport);
bool raylib_lite_tilemap_is_blocked(raylib_lite_tilemap_t map,int tile_x,int tile_y);
bool raylib_lite_tilemap_find_object(raylib_lite_tilemap_t map,mosaico_asset_id_t id,raylib_lite_map_object_t *out);
bool raylib_lite_tilemap_object_at(raylib_lite_tilemap_t map,size_t index,raylib_lite_map_object_t *out);
size_t raylib_lite_tilemap_path_points(raylib_lite_tilemap_t map,const Vector2 **out_points);
void raylib_lite_tilemap_unload(raylib_lite_tilemap_t map);
#ifdef __cplusplus
}
#endif
