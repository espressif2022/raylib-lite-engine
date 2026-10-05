// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

typedef enum {
    MOSAICO_GAME_FRAME_ACCEPTED = 0,
    MOSAICO_GAME_FRAME_BUSY,
    MOSAICO_GAME_FRAME_SUPERSEDED,
    MOSAICO_GAME_FRAME_DISPLAY_ERROR,
} mosaico_game_frame_result_t;
