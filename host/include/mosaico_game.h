// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

#define MOSAICO_GAME_WIDTH 480
#define MOSAICO_GAME_HEIGHT 480

typedef enum {
    MOSAICO_GAME_FRAME_ACCEPTED = 0,
    MOSAICO_GAME_FRAME_BUSY,
    MOSAICO_GAME_FRAME_SUPERSEDED,
    MOSAICO_GAME_FRAME_DISPLAY_ERROR,
} mosaico_game_frame_result_t;
