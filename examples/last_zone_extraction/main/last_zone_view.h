// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "raylib_lite_2d.h"
#include "last_zone_game.h"

typedef struct {
    uint32_t acquire_us;
    uint32_t raycast_us;
    uint32_t sky_us;
    uint32_t floor_us;
    uint32_t wall_us;
    uint32_t grade_us;
    uint32_t sprites_us;
    uint32_t hud_us;
    uint32_t submit_us;
    uint32_t total_us;
    uint16_t rays_cast;
    uint16_t refined_columns;
} last_zone_view_stats_t;

void last_zone_view_render(const last_zone_game_t *game,raylib_lite_atlas_t enemies,
                           raylib_lite_atlas_t weapon,raylib_lite_atlas_t environment,
                           raylib_lite_atlas_t floor,raylib_lite_wall_atlas_t walls,raylib_lite_atlas_t controls,
                           raylib_lite_atlas_t props);
void last_zone_view_get_stats(last_zone_view_stats_t *stats);
