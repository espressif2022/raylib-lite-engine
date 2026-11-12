// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_game_audio.h"
#include "platform_game.h"

typedef struct {
    void (*init)(void);
    void (*pulse)(uint8_t strength, uint16_t duration_ms);
    void (*pattern)(uint8_t first_strength, uint16_t first_ms,
                    uint8_t second_strength, uint16_t gap_ms,
                    uint16_t second_ms);
    void (*stop)(void);
} sky_hop_haptics_t;

enum {
    SKY_HOP_SOUND_JUMP,
    SKY_HOP_SOUND_COIN,
    SKY_HOP_SOUND_STOMP,
    SKY_HOP_SOUND_HURT,
    SKY_HOP_SOUND_CLEAR,
    SKY_HOP_SOUND_SPRING,
    SKY_HOP_SOUND_CHECKPOINT,
    SKY_HOP_SOUND_LAND,
    SKY_HOP_SOUND_GAME_OVER,
    SKY_HOP_SOUND_START,
    SKY_HOP_SOUND_COUNT,
};

enum {
    SKY_HOP_MUSIC_DAY,
    SKY_HOP_MUSIC_NIGHT,
    SKY_HOP_MUSIC_COUNT,
};

typedef struct {
    const sky_hop_haptics_t *haptics;
    Sound sounds[SKY_HOP_SOUND_COUNT];
    Music music[SKY_HOP_MUSIC_COUNT];
    int8_t playing_track;
    bool audio_initialized;
} sky_hop_feedback_t;

void sky_hop_feedback_init(sky_hop_feedback_t *feedback, const sky_hop_haptics_t *haptics);
/* Plays each consumed model event once; events come from platform_game_take_events(). */
void sky_hop_feedback_events(sky_hop_feedback_t *feedback, const platform_game_t *game,
                             uint32_t events);
void sky_hop_feedback_music(sky_hop_feedback_t *feedback, const platform_game_t *game,
                            bool suspended);
void sky_hop_feedback_close(sky_hop_feedback_t *feedback);
