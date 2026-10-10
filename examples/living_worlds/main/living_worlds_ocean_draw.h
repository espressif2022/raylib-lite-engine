// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_2d.h"
#include "living_worlds_ocean.h"
void living_ocean_draw(const living_ocean_t *ocean,float yaw,float pitch,
                           uint8_t effects_level,raylib_lite_atlas_t water,
                           raylib_lite_atlas_t left_front,raylib_lite_atlas_t left_side,
                           raylib_lite_atlas_t left_rear,raylib_lite_atlas_t right_front,
                           raylib_lite_atlas_t right_side,raylib_lite_atlas_t right_rear);

/* Residual emit costs exclude raster loops, water mesh setup and jelly drawing.
 * Valid only with CONFIG_RAYLIB_LITE_RASTER_PROFILE; otherwise zero. */
typedef struct {
    uint32_t water_emit_us, reefs_emit_us, jelly_us;
} living_ocean_draw_profile_t;
living_ocean_draw_profile_t living_ocean_draw_profile(void);
