// SPDX-License-Identifier: Apache-2.0
#include "shooter_feedback.h"

static const char *const SOUND_PATHS[SHOOTER_SOUND_COUNT] = {
    [SHOOTER_SOUND_SHOT] = "shot.sound",
    [SHOOTER_SOUND_EXPLODE] = "explode.sound",
    [SHOOTER_SOUND_HURT] = "hurt.sound",
    [SHOOTER_SOUND_WAVE] = "wave.sound",
    [SHOOTER_SOUND_POWER] = "power.sound",
    [SHOOTER_SOUND_START] = "start.sound",
    [SHOOTER_SOUND_GAME_OVER] = "game_over.sound",
};

static void play(const shooter_feedback_t *feedback, unsigned sound)
{
    if (feedback->sounds[sound].frameCount)
        raylib_lite_game_audio_play_sound(feedback->sounds[sound]);
}

static void pulse(const shooter_feedback_t *feedback, uint8_t strength, uint16_t ms)
{
    if (feedback->haptics && feedback->haptics->pulse)
        feedback->haptics->pulse(strength, ms);
}

static void pattern(const shooter_feedback_t *feedback, uint8_t first, uint16_t first_ms,
                    uint8_t second, uint16_t gap_ms, uint16_t second_ms)
{
    if (feedback->haptics && feedback->haptics->pattern)
        feedback->haptics->pattern(first, first_ms, second, gap_ms, second_ms);
}

static shooter_feedback_snapshot_t snapshot(const shooter_game_t *game)
{
    return (shooter_feedback_snapshot_t){
        .shots_fired = game->shots_fired, .kills = game->kills, .lives = game->lives,
        .wave = game->wave, .combo = game->combo, .phase = game->phase};
}

void shooter_feedback_init(shooter_feedback_t *feedback, const shooter_game_t *game,
                           const shooter_haptics_t *haptics)
{
    *feedback = (shooter_feedback_t){.haptics = haptics, .last = snapshot(game)};
    if (haptics && haptics->init) haptics->init();
    raylib_lite_game_audio_init();
    feedback->audio_initialized = true;
    if (!raylib_lite_game_audio_ready()) return;
    for (unsigned i = 0; i < SHOOTER_SOUND_COUNT; ++i)
        feedback->sounds[i] = raylib_lite_game_audio_load_sound(SOUND_PATHS[i]);
    /* The cannon fires every few ticks; keep it under the explosions. */
    if (feedback->sounds[SHOOTER_SOUND_SHOT].frameCount)
        raylib_lite_game_audio_set_sound_volume(feedback->sounds[SHOOTER_SOUND_SHOT], .35f);
    feedback->music = raylib_lite_game_audio_load_music("music.sound");
}

void shooter_feedback_sync(shooter_feedback_t *feedback, const shooter_game_t *game)
{
    feedback->last = snapshot(game);
    if (feedback->haptics && feedback->haptics->stop) feedback->haptics->stop();
}

void shooter_feedback_update(shooter_feedback_t *feedback, const shooter_game_t *game,
                             bool suspended)
{
    const shooter_feedback_snapshot_t now = snapshot(game);
    const shooter_feedback_snapshot_t was = feedback->last;
    feedback->last = now;
    if (now.phase == SHOOTER_GAME_OVER && was.phase != SHOOTER_GAME_OVER) {
        play(feedback, SHOOTER_SOUND_GAME_OVER);
        pattern(feedback, 95, 80, 70, 40, 140);
    } else if (now.lives < was.lives) {
        play(feedback, SHOOTER_SOUND_HURT);
        pattern(feedback, 80, 40, 50, 30, 40);
    } else if (now.phase == SHOOTER_PLAYING && was.phase == SHOOTER_START) {
        play(feedback, SHOOTER_SOUND_START);
        pulse(feedback, 30, 20);
    } else if (now.wave > was.wave) {
        play(feedback, SHOOTER_SOUND_WAVE);
        pattern(feedback, 30, 20, 45, 30, 20);
    } else if ((now.combo >= 4 && was.combo < 4) || (now.combo >= 8 && was.combo < 8)) {
        play(feedback, SHOOTER_SOUND_POWER);
        pulse(feedback, 34, 18);
    } else if (now.kills > was.kills) {
        play(feedback, SHOOTER_SOUND_EXPLODE);
        pulse(feedback, 40, 22);
    } else if (now.shots_fired > was.shots_fired) {
        play(feedback, SHOOTER_SOUND_SHOT);
    }
    if (now.phase == SHOOTER_PAUSED && was.phase != SHOOTER_PAUSED &&
        feedback->haptics && feedback->haptics->stop)
        feedback->haptics->stop();

    const bool want = now.phase == SHOOTER_PLAYING || now.phase == SHOOTER_PAUSED;
    if (!feedback->music.frameCount) return;
    if (want != feedback->music_playing) {
        if (want) raylib_lite_game_audio_play_music(feedback->music);
        else raylib_lite_game_audio_stop_music(feedback->music);
        feedback->music_playing = want;
    }
    if (!want) return;
    raylib_lite_game_audio_set_music_volume(
        feedback->music, suspended || now.phase == SHOOTER_PAUSED ? .05f : .2f);
    raylib_lite_game_audio_update_music(feedback->music);
}

void shooter_feedback_close(shooter_feedback_t *feedback)
{
    if (feedback->haptics && feedback->haptics->stop) feedback->haptics->stop();
    if (!feedback->audio_initialized) return;
    if (feedback->music.frameCount) {
        if (feedback->music_playing) raylib_lite_game_audio_stop_music(feedback->music);
        raylib_lite_game_audio_unload_music(feedback->music);
        feedback->music = (Music){0};
    }
    for (unsigned i = 0; i < SHOOTER_SOUND_COUNT; ++i) {
        if (!feedback->sounds[i].frameCount) continue;
        raylib_lite_game_audio_unload_sound(feedback->sounds[i]);
        feedback->sounds[i] = (Sound){0};
    }
    raylib_lite_game_audio_close();
    feedback->audio_initialized = false;
    feedback->music_playing = false;
}
