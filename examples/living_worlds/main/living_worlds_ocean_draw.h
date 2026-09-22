// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "living_worlds_ocean.h"
void living_ocean_draw(const living_ocean_t *ocean,float yaw,float pitch,
                           uint8_t effects_level,MosaicoAtlas water,
                           MosaicoAtlas left_front,MosaicoAtlas left_side,
                           MosaicoAtlas left_rear,MosaicoAtlas right_front,
                           MosaicoAtlas right_side,MosaicoAtlas right_rear);

/* Residual emit costs exclude raster loops, water mesh setup and jelly drawing.
 * Valid only with CONFIG_MOSAICO_GAME_RASTER_PROFILE; otherwise zero. */
typedef struct {
    uint32_t water_emit_us, reefs_emit_us, jelly_us;
} living_ocean_draw_profile_t;
living_ocean_draw_profile_t living_ocean_draw_profile(void);
