// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_game_audio.h"
#include "living_worlds_world.h"

enum { SCENE_AUDIO_COUNT = LIVING_SCENE_RAINFOREST + 1 };

extern const char *const SCENE_AUDIO_PATHS[SCENE_AUDIO_COUNT];

typedef struct {
    Music tracks[SCENE_AUDIO_COUNT];
    uint8_t playing_scene;
    bool load_attempted;
} living_worlds_scene_audio_t;

void living_worlds_scene_audio_init(living_worlds_scene_audio_t *audio);
void living_worlds_scene_audio_select(living_worlds_scene_audio_t *audio,
                                      uint8_t scene);
/* Returns true on the first update that attempts to load the tracks. */
bool living_worlds_scene_audio_update(living_worlds_scene_audio_t *audio,
                                      uint8_t scene);
void living_worlds_scene_audio_close(living_worlds_scene_audio_t *audio);
