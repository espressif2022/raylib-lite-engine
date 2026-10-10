// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_2d.h"
#include "living_worlds_world.h"

typedef struct {
    raylib_lite_atlas_t aurora,sunrise,ocean,rainforest,rainforest_falls;
    raylib_lite_atlas_t sunrise_cliff_front,sunrise_cliff_side,sunrise_cliff_rear;
    raylib_lite_atlas_t aurora_ice_front,aurora_ice_side,aurora_ice_rear;
    raylib_lite_atlas_t ocean_left_front,ocean_left_side,ocean_left_rear;
    raylib_lite_atlas_t ocean_right_front,ocean_right_side,ocean_right_rear;
} living_worlds_atlases_t;

void living_worlds_view_render(const living_world_t *world,
                            const living_worlds_atlases_t *atlases);
