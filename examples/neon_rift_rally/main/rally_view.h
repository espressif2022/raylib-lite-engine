// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_2d.h"
#include "rally_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float steering;
    int8_t pose; /* -2 heavy left .. +2 heavy right */
    uint8_t hold_ticks;
    uint8_t alert;
    uint8_t alert_ticks;
} rally_view_motion_t;

void rally_view_motion_reset(rally_view_motion_t *motion);
void rally_view_motion_update(rally_view_motion_t *motion, const rally_game_t *game);

/* The view samples the same track-local model as gameplay. */
int rally_view_render(const rally_game_t *game, raylib_lite_atlas_t rally_art,
                      raylib_lite_atlas_t track_background,
                      raylib_lite_atlas_t motorcycle_art, unsigned course_id,
                      const rally_view_motion_t *motion);

#if defined(RAYLIB_LITE_HOST_SIMULATION)
/* Last render's sky, road, scenery, actors/effects and HUD times in us. */
void rally_view_get_host_profile(uint32_t out[5]);
#endif

#ifdef __cplusplus
}
#endif
