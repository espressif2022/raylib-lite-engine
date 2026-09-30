// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "mosaico_game_audio.h"
#include "last_zone_game.h"

typedef struct {
    void (*init)(void);
    void (*pulse)(uint8_t strength, uint16_t duration_ms);
    void (*pattern)(uint8_t first_strength, uint16_t first_ms,
                    uint8_t second_strength, uint16_t gap_ms,
                    uint16_t second_ms);
    void (*stop)(void);
    void (*missing_asset)(const char *path);
} last_zone_feedback_backend_t;

enum {
    LAST_ZONE_SOUND_RIFLE,
    LAST_ZONE_SOUND_IMPACT,
    LAST_ZONE_SOUND_CONFIRM,
    LAST_ZONE_SOUND_HURT,
    LAST_ZONE_SOUND_EMPTY,
    LAST_ZONE_SOUND_PICKUP,
    LAST_ZONE_SOUND_ALERT,
    LAST_ZONE_SOUND_STEP_L,
    LAST_ZONE_SOUND_STEP_R,
    LAST_ZONE_SOUND_EXPLODE,
    LAST_ZONE_SOUND_EXTRACT,
    LAST_ZONE_SOUND_COUNT,
};

typedef struct {
    const last_zone_feedback_backend_t *backend;
    Sound sounds[LAST_ZONE_SOUND_COUNT];
    Music music;
    uint32_t processed_tick;
    uint8_t previous_hp, previous_armor;
    uint8_t step_wait, enemy_step_wait;
    last_zone_phase_t previous_phase;
    bool step_right, audio_initialized, load_attempted;
} last_zone_feedback_t;

void last_zone_feedback_init(last_zone_feedback_t *feedback,
                             const last_zone_game_t *game,
                             const last_zone_feedback_backend_t *backend);
void last_zone_feedback_reset(last_zone_feedback_t *feedback,
                              const last_zone_game_t *game);
void last_zone_feedback_music(last_zone_feedback_t *feedback,
                              last_zone_phase_t phase);
void last_zone_feedback_events(last_zone_feedback_t *feedback,
                               const last_zone_game_t *game);
void last_zone_feedback_close(last_zone_feedback_t *feedback);
