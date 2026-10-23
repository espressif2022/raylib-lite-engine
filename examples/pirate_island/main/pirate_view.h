// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_2d.h"
#include "raylib_lite_raylib.h"
#include "pirate_game.h"

void pirate_view_render(const pirate_game_t *g, raylib_lite_atlas_t bg_atlas,
                        raylib_lite_atlas_t spr_atlas, Font zh_font);
