// SPDX-License-Identifier: Apache-2.0
#include "sky_hop_feedback.h"

static const char *const SOUND_PATHS[SKY_HOP_SOUND_COUNT] = {
    [SKY_HOP_SOUND_JUMP] = "jump.sound",
    [SKY_HOP_SOUND_COIN] = "coin.sound",
    [SKY_HOP_SOUND_STOMP] = "stomp.sound",
    [SKY_HOP_SOUND_HURT] = "hurt.sound",
    [SKY_HOP_SOUND_CLEAR] = "win.sound",
    [SKY_HOP_SOUND_SPRING] = "spring.sound",
    [SKY_HOP_SOUND_CHECKPOINT] = "checkpoint.sound",
    [SKY_HOP_SOUND_LAND] = "land.sound",
    [SKY_HOP_SOUND_GAME_OVER] = "game_over.sound",
    [SKY_HOP_SOUND_START] = "start.sound",
};

static const char *const MUSIC_PATHS[SKY_HOP_MUSIC_COUNT] = {
    [SKY_HOP_MUSIC_DAY] = "music.sound",
    [SKY_HOP_MUSIC_NIGHT] = "music_night.sound",
};

static void pulse(const sky_hop_feedback_t *feedback, uint8_t strength, uint16_t ms)
{
    if (feedback->haptics && feedback->haptics->pulse)
        feedback->haptics->pulse(strength, ms);
}

static void pattern(const sky_hop_feedback_t *feedback, uint8_t first, uint16_t first_ms,
                    uint8_t second, uint16_t gap_ms, uint16_t second_ms)
{
    if (feedback->haptics && feedback->haptics->pattern)
        feedback->haptics->pattern(first, first_ms, second, gap_ms, second_ms);
}

static void play(const sky_hop_feedback_t *feedback, unsigned sound)
{
    if (feedback->sounds[sound].frameCount)
        raylib_lite_game_audio_play_sound(feedback->sounds[sound]);
}

void sky_hop_feedback_init(sky_hop_feedback_t *feedback, const sky_hop_haptics_t *haptics)
{
    *feedback = (sky_hop_feedback_t){.haptics = haptics, .playing_track = -1};
    if (haptics && haptics->init) haptics->init();
    raylib_lite_game_audio_init();
    feedback->audio_initialized = true;
    if (!raylib_lite_game_audio_ready()) return;
    for (unsigned i = 0; i < SKY_HOP_SOUND_COUNT; ++i)
        feedback->sounds[i] = raylib_lite_game_audio_load_sound(SOUND_PATHS[i]);
    for (unsigned i = 0; i < SKY_HOP_MUSIC_COUNT; ++i)
        feedback->music[i] = raylib_lite_game_audio_load_music(MUSIC_PATHS[i]);
}

void sky_hop_feedback_events(sky_hop_feedback_t *feedback, const platform_game_t *game,
                             uint32_t events)
{
    if (!events) return;
    /* One audible cue per tick, chosen by importance; haptics follow the cue. */
    if (events & PLATFORM_EVENT_GAME_OVER) {
        play(feedback, SKY_HOP_SOUND_GAME_OVER);
        pattern(feedback, 90, 70, 60, 60, 120);
    } else if (events & (PLATFORM_EVENT_WON | PLATFORM_EVENT_LEVEL_CLEAR)) {
        play(feedback, SKY_HOP_SOUND_CLEAR);
        pattern(feedback, 50, 30, 80, 40, 60);
    } else if (events & PLATFORM_EVENT_HURT) {
        play(feedback, SKY_HOP_SOUND_HURT);
        pattern(feedback, 85, 45, 50, 30, 40);
    } else if (events & PLATFORM_EVENT_STOMP) {
        play(feedback, SKY_HOP_SOUND_STOMP);
        pulse(feedback, (uint8_t)(40 + 12 * game->stomp_chain), 26);
    } else if (events & PLATFORM_EVENT_SPRING) {
        play(feedback, SKY_HOP_SOUND_SPRING);
        pulse(feedback, 55, 30);
    } else if (events & PLATFORM_EVENT_CHECKPOINT) {
        play(feedback, SKY_HOP_SOUND_CHECKPOINT);
        pattern(feedback, 30, 20, 45, 30, 20);
    } else if (events & PLATFORM_EVENT_COIN) {
        play(feedback, SKY_HOP_SOUND_COIN);
        pulse(feedback, 18, 12);
    } else if (events & PLATFORM_EVENT_START) {
        play(feedback, SKY_HOP_SOUND_START);
        pulse(feedback, 30, 20);
    } else if (events & PLATFORM_EVENT_JUMP) {
        play(feedback, SKY_HOP_SOUND_JUMP);
    } else if (events & PLATFORM_EVENT_LAND) {
        play(feedback, SKY_HOP_SOUND_LAND);
        if (game->land_air_ticks > 24U) pulse(feedback, 34, 18);
    }
    if ((events & PLATFORM_EVENT_PAUSE) && feedback->haptics && feedback->haptics->stop)
        feedback->haptics->stop();
}

void sky_hop_feedback_music(sky_hop_feedback_t *feedback, const platform_game_t *game,
                            bool suspended)
{
    bool want = !suspended && (game->phase == PLATFORM_PLAYING ||
                               game->phase == PLATFORM_PAUSED);
    int8_t track = want ? (int8_t)(game->level & 1U ? SKY_HOP_MUSIC_NIGHT
                                                    : SKY_HOP_MUSIC_DAY) : -1;
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
        game->phase == PLATFORM_PAUSED ? .06f : .22f);
    raylib_lite_game_audio_update_music(feedback->music[track]);
}

void sky_hop_feedback_close(sky_hop_feedback_t *feedback)
{
    if (feedback->haptics && feedback->haptics->stop) feedback->haptics->stop();
    if (!feedback->audio_initialized) return;
    for (unsigned i = 0; i < SKY_HOP_MUSIC_COUNT; ++i) {
        if (!feedback->music[i].frameCount) continue;
        raylib_lite_game_audio_stop_music(feedback->music[i]);
        raylib_lite_game_audio_unload_music(feedback->music[i]);
        feedback->music[i] = (Music){0};
    }
    for (unsigned i = 0; i < SKY_HOP_SOUND_COUNT; ++i) {
        if (!feedback->sounds[i].frameCount) continue;
        raylib_lite_game_audio_unload_sound(feedback->sounds[i]);
        feedback->sounds[i] = (Sound){0};
    }
    raylib_lite_game_audio_close();
    feedback->audio_initialized = false;
    feedback->playing_track = -1;
}
