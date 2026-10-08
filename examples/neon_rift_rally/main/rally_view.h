// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_2d.h"
#include "rally_game.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The view samples the same track-local model as gameplay. */
int rally_view_render(const rally_game_t *game, raylib_lite_atlas_t rally_art,
                      raylib_lite_atlas_t track_background);

#ifdef __cplusplus
}
#endif
