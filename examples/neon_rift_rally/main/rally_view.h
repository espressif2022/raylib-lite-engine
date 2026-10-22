// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_2d.h"
#include "rally_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RALLY_CALLOUT_NONE,
    RALLY_CALLOUT_NEAR_MISS,
    RALLY_CALLOUT_DRIFT_BOOST,
    RALLY_CALLOUT_CHECKPOINT,
    RALLY_CALLOUT_LAP,
    RALLY_CALLOUT_FINAL_LAP,
    RALLY_CALLOUT_RIVAL_BOOST, /* callout_value: opponent index */
    RALLY_CALLOUT_RIVAL_SLIP,
} rally_callout_t;

typedef struct {
    float steering;
    int8_t pose; /* -2 heavy left .. +2 heavy right */
    uint8_t hold_ticks;
    uint8_t alert; /* 1 overtake, 2 overtaken, 3 draft ready */
    uint8_t alert_ticks;
    uint8_t callout; /* rally_callout_t */
    uint8_t callout_ticks;
    uint32_t callout_value;
    uint32_t last_score;
    uint32_t shown_score; /* rolls toward the model score for the HUD */
    uint8_t boost_cooldown; /* spaces out callouts for other riders' nitro */
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
