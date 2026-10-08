// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include "raylib_lite_result.h"

typedef enum {
    TOWER_AUDIO_START,
    TOWER_AUDIO_BUILD,
    TOWER_AUDIO_SHOT,
    TOWER_AUDIO_KILL,
    TOWER_AUDIO_WAVE,
    TOWER_AUDIO_LEAK,
    TOWER_AUDIO_GAME_OVER,
} tower_audio_cue_t;

raylib_lite_result_t tower_audio_init(void);
raylib_lite_result_t tower_audio_play(tower_audio_cue_t cue);
void tower_audio_set_music(bool paused, bool game_over);
