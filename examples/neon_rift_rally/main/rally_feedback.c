// SPDX-License-Identifier: Apache-2.0
#include "rally_feedback.h"

static const char *const SOUND_PATHS[RALLY_SOUND_COUNT] = {
    [RALLY_SOUND_COUNTDOWN] = "countdown.sound",
    [RALLY_SOUND_GO] = "go.sound",
    [RALLY_SOUND_BOOST] = "boost.sound",
    [RALLY_SOUND_DRIFT] = "drift.sound",
    [RALLY_SOUND_JUMP] = "jump.sound",
    [RALLY_SOUND_LAND] = "land.sound",
    [RALLY_SOUND_CHECKPOINT] = "checkpoint.sound",
    [RALLY_SOUND_LAP] = "lap.sound",
    [RALLY_SOUND_FINISH] = "finish.sound",
    [RALLY_SOUND_FAIL] = "fail.sound",
    [RALLY_SOUND_OFFTRACK] = "offtrack.sound",
    [RALLY_SOUND_COLLISION] = "collision.sound",
    [RALLY_SOUND_OVERTAKE] = "overtake.sound",
    [RALLY_SOUND_OVERTAKEN] = "overtaken.sound",
    [RALLY_SOUND_NEAR_MISS] = "near_miss.sound",
    [RALLY_SOUND_DRAFT] = "draft.sound",
    [RALLY_SOUND_RIVAL] = "rival.sound",
};

/* Indexed by course id: Neon Loop, Sunset Sprint, Polar Rift. */
static const char *const MUSIC_PATHS[RALLY_MUSIC_COUNT] = {
    "race_neon.sound", "race_sunset.sound", "race_polar.sound",
};

static void pulse(const rally_feedback_t *feedback, uint8_t strength, uint16_t ms)
{
    if (feedback->haptics && feedback->haptics->pulse)
        feedback->haptics->pulse(strength, ms);
}

static void pattern(const rally_feedback_t *feedback, uint8_t first, uint16_t first_ms,
                    uint8_t second, uint16_t gap_ms, uint16_t second_ms)
{
    if (feedback->haptics && feedback->haptics->pattern)
        feedback->haptics->pattern(first, first_ms, second, gap_ms, second_ms);
}

static void stop_haptics(const rally_feedback_t *feedback)
{
    if (feedback->haptics && feedback->haptics->stop) feedback->haptics->stop();
}

static void play(const rally_feedback_t *feedback, unsigned sound)
{
    if (feedback->sounds[sound].frameCount)
        raylib_lite_game_audio_play_sound(feedback->sounds[sound]);
}

void rally_feedback_init(rally_feedback_t *feedback, const rally_haptics_t *haptics)
{
    *feedback = (rally_feedback_t){.haptics = haptics, .playing_track = -1};
    if (haptics && haptics->init) haptics->init();
    raylib_lite_game_audio_init();
    feedback->audio_initialized = true;
    if (!raylib_lite_game_audio_ready()) return;
    for (unsigned i = 0; i < RALLY_SOUND_COUNT; ++i)
        feedback->sounds[i] = raylib_lite_game_audio_load_sound(SOUND_PATHS[i]);
    for (unsigned i = 0; i < RALLY_MUSIC_COUNT; ++i)
        feedback->music[i] = raylib_lite_game_audio_load_music(MUSIC_PATHS[i]);
}

void rally_feedback_reset(rally_feedback_t *feedback)
{
    stop_haptics(feedback);
    feedback->last_serial = 0;
    feedback->cooldown = 0;
    feedback->countdown_digit = 0;
}

void rally_feedback_events(rally_feedback_t *feedback, const rally_game_t *game)
{
    if (game->event_serial == feedback->last_serial) return;
    feedback->last_serial = game->event_serial;
    const uint32_t events = rally_events(game);
    if (events & RALLY_EVENT_FAIL) {
        play(feedback, RALLY_SOUND_FAIL); pattern(feedback, 100, 90, 76, 90, 70);
    } else if (events & RALLY_EVENT_COLLISION) {
        play(feedback, RALLY_SOUND_COLLISION); pattern(feedback, 100, 55, 82, 46, 28);
    } else if (events & RALLY_EVENT_FINISH) {
        play(feedback, RALLY_SOUND_FINISH); pattern(feedback, 74, 65, 100, 90, 100);
    } else if (events & RALLY_EVENT_LAP) {
        play(feedback, RALLY_SOUND_LAP); pattern(feedback, 48, 35, 58, 40, 35);
    } else if (events & RALLY_EVENT_LANDING) {
        play(feedback, RALLY_SOUND_LAND); pattern(feedback, 76, 38, 34, 20, 18);
    } else if (events & RALLY_EVENT_OVERTAKE) {
        play(feedback, RALLY_SOUND_OVERTAKE); pulse(feedback, 46, 24);
    } else if (events & RALLY_EVENT_OVERTAKEN) {
        play(feedback, RALLY_SOUND_OVERTAKEN); pattern(feedback, 28, 18, 24, 16, 24);
    } else if (events & RALLY_EVENT_NEAR_MISS) {
        play(feedback, RALLY_SOUND_NEAR_MISS); pulse(feedback, 34, 18);
    } else if (events & RALLY_EVENT_OFFTRACK) {
        play(feedback, RALLY_SOUND_OFFTRACK); pattern(feedback, 58, 45, 30, 34, 24);
    } else if (events & RALLY_EVENT_START) {
        play(feedback, RALLY_SOUND_GO); pattern(feedback, 28, 25, 44, 24, 20);
    } else if (!feedback->cooldown) {
        bool consumed = true;
        if (events & RALLY_EVENT_CHECKPOINT) {
            play(feedback, RALLY_SOUND_CHECKPOINT); pulse(feedback, 28, 25);
        } else if (events & RALLY_EVENT_DRAFT_READY) {
            play(feedback, RALLY_SOUND_DRAFT); pulse(feedback, 18, 16);
        } else if (events & RALLY_EVENT_NITRO) {
            play(feedback, RALLY_SOUND_BOOST); pattern(feedback, 42, 24, 26, 18, 16);
        } else if (events & RALLY_EVENT_DRIFT_BOOST) {
            play(feedback, RALLY_SOUND_BOOST); pulse(feedback, 26, 18);
        } else if (events & RALLY_EVENT_DRIFT) {
            play(feedback, RALLY_SOUND_DRIFT); pulse(feedback, 20, 16);
        } else if (events & RALLY_EVENT_JUMP) {
            play(feedback, RALLY_SOUND_JUMP); pulse(feedback, 30, 22);
        } else if (events & RALLY_EVENT_RIVAL_BOOST) {
            play(feedback, RALLY_SOUND_RIVAL); pulse(feedback, 16, 20);
        } else if (events & RALLY_EVENT_RIVAL_SLIP) {
            pulse(feedback, 22, 14);
        } else consumed = false;
        if (consumed) feedback->cooldown = 4U;
    }
}

static float music_volume(const rally_game_t *game, bool paused)
{
    if (paused) return .04f;
    if (game->phase == RALLY_PHASE_COUNTDOWN) return .12f;
    float ratio = game->speed / RALLY_MAX_SPEED;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    return .15f + .11f * ratio + (game->nitro_active ? .04f : 0.0f);
}

void rally_feedback_update(rally_feedback_t *feedback, const rally_game_t *game,
                           unsigned course_id, bool paused)
{
    if (!paused && feedback->cooldown) --feedback->cooldown;
    if (game->phase == RALLY_PHASE_COUNTDOWN && !paused) {
        const uint8_t digit = (uint8_t)((game->countdown_ticks + 29U) / 30U);
        if (digit && digit != feedback->countdown_digit) {
            play(feedback, RALLY_SOUND_COUNTDOWN);
            pulse(feedback, 16, 12);
        }
        feedback->countdown_digit = digit;
    }
    const bool racing = game->phase == RALLY_PHASE_COUNTDOWN ||
                        game->phase == RALLY_PHASE_PLAYING;
    const int8_t track = racing ? (int8_t)(course_id % RALLY_MUSIC_COUNT) : -1;
    if (track != feedback->playing_track) {
        if (feedback->playing_track >= 0 &&
            feedback->music[feedback->playing_track].frameCount)
            raylib_lite_game_audio_stop_music(feedback->music[feedback->playing_track]);
        if (track >= 0 && feedback->music[track].frameCount)
            raylib_lite_game_audio_play_music(feedback->music[track]);
        feedback->playing_track = track;
    }
    if (track < 0 || !feedback->music[track].frameCount) return;
    raylib_lite_game_audio_set_music_volume(feedback->music[track],
                                            music_volume(game, paused));
    raylib_lite_game_audio_update_music(feedback->music[track]);
}

void rally_feedback_pause(rally_feedback_t *feedback)
{
    stop_haptics(feedback);
}

void rally_feedback_close(rally_feedback_t *feedback)
{
    stop_haptics(feedback);
    if (!feedback->audio_initialized) return;
    for (unsigned i = 0; i < RALLY_MUSIC_COUNT; ++i) {
        if (!feedback->music[i].frameCount) continue;
        raylib_lite_game_audio_stop_music(feedback->music[i]);
        raylib_lite_game_audio_unload_music(feedback->music[i]);
        feedback->music[i] = (Music){0};
    }
    for (unsigned i = 0; i < RALLY_SOUND_COUNT; ++i) {
        if (!feedback->sounds[i].frameCount) continue;
        raylib_lite_game_audio_unload_sound(feedback->sounds[i]);
        feedback->sounds[i] = (Sound){0};
    }
    raylib_lite_game_audio_close();
    feedback->audio_initialized = false;
    feedback->playing_track = -1;
}
