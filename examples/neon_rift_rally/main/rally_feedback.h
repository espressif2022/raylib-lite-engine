// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "raylib_lite_game_audio.h"
#include "rally_game.h"

typedef struct {
    void (*init)(void);
    void (*pulse)(uint8_t strength, uint16_t duration_ms);
    void (*pattern)(uint8_t first_strength, uint16_t first_ms,
                    uint8_t second_strength, uint16_t gap_ms,
                    uint16_t second_ms);
    void (*stop)(void);
} rally_haptics_t;

enum {
    RALLY_SOUND_COUNTDOWN,
    RALLY_SOUND_GO,
    RALLY_SOUND_BOOST,
    RALLY_SOUND_DRIFT,
    RALLY_SOUND_JUMP,
    RALLY_SOUND_LAND,
    RALLY_SOUND_CHECKPOINT,
    RALLY_SOUND_LAP,
    RALLY_SOUND_FINISH,
    RALLY_SOUND_FAIL,
    RALLY_SOUND_OFFTRACK,
    RALLY_SOUND_COLLISION,
    RALLY_SOUND_OVERTAKE,
    RALLY_SOUND_OVERTAKEN,
    RALLY_SOUND_NEAR_MISS,
    RALLY_SOUND_DRAFT,
    RALLY_SOUND_RIVAL,
    RALLY_SOUND_COUNT,
};

#define RALLY_MUSIC_COUNT 3U

typedef struct {
    const rally_haptics_t *haptics;
    Sound sounds[RALLY_SOUND_COUNT];
    Music music[RALLY_MUSIC_COUNT];
    int8_t playing_track;
    uint8_t countdown_digit;
    uint8_t cooldown;
    uint32_t last_serial;
    bool audio_initialized;
} rally_feedback_t;

void rally_feedback_init(rally_feedback_t *feedback, const rally_haptics_t *haptics);
/* Forget consumed events after a race reset; stops any running haptic. */
void rally_feedback_reset(rally_feedback_t *feedback);
/* Plays one prioritized cue per new event serial; low-priority cues share a cooldown. */
void rally_feedback_events(rally_feedback_t *feedback, const rally_game_t *game);
/* Per tick: countdown beeps, course music, speed-following volume and pause ducking. */
void rally_feedback_update(rally_feedback_t *feedback, const rally_game_t *game,
                           unsigned course_id, bool paused);
void rally_feedback_pause(rally_feedback_t *feedback);
void rally_feedback_close(rally_feedback_t *feedback);
