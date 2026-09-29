// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "tomb_game.h"

typedef struct {
    bool stick_active;
    bool jump_active;
    int stick_x;
    int stick_y;
    float display_fps;
} tomb_hud_input_t;

void tomb_view_render(const tomb_game_t *game, MosaicoWallAtlas textures, MosaicoAtlas controls,
                      const tomb_hud_input_t *input);
