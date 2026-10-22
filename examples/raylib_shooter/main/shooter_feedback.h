// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_game_audio.h"
#include "shooter_game.h"

typedef struct {
    void (*init)(void);
    void (*pulse)(uint8_t strength, uint16_t duration_ms);
    void (*pattern)(uint8_t first_strength, uint16_t first_ms,
                    uint8_t second_strength, uint16_t gap_ms,
                    uint16_t second_ms);
    void (*stop)(void);
} shooter_haptics_t;

enum {
    SHOOTER_SOUND_SHOT,
    SHOOTER_SOUND_EXPLODE,
    SHOOTER_SOUND_HURT,
    SHOOTER_SOUND_WAVE,
    SHOOTER_SOUND_POWER,
    SHOOTER_SOUND_START,
    SHOOTER_SOUND_GAME_OVER,
    SHOOTER_SOUND_COUNT,
};

/* The model exposes no event flags, so cues come from tick-to-tick deltas. */
typedef struct {
    uint32_t shots_fired;
    uint16_t kills;
    uint8_t lives, wave, combo;
    shooter_phase_t phase;
} shooter_feedback_snapshot_t;

typedef struct {
    const shooter_haptics_t *haptics;
    Sound sounds[SHOOTER_SOUND_COUNT];
    Music music;
    shooter_feedback_snapshot_t last;
    bool music_playing;
    bool audio_initialized;
} shooter_feedback_t;

void shooter_feedback_init(shooter_feedback_t *feedback, const shooter_game_t *game,
                           const shooter_haptics_t *haptics);
/* Re-baseline after a reset so the new run does not replay stale deltas. */
void shooter_feedback_sync(shooter_feedback_t *feedback, const shooter_game_t *game);
void shooter_feedback_update(shooter_feedback_t *feedback, const shooter_game_t *game,
                             bool suspended);
void shooter_feedback_close(shooter_feedback_t *feedback);
