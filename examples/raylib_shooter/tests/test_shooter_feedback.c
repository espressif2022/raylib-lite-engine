// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "shooter_feedback.h"

static unsigned s_loaded, s_unloaded, s_closed, s_haptics, s_haptic_stops;
static unsigned s_played[SHOOTER_SOUND_COUNT + 1];
static unsigned s_music_plays, s_music_stops;
static float s_music_volume;

void raylib_lite_game_audio_init(void) {}
void raylib_lite_game_audio_close(void) { ++s_closed; }
bool raylib_lite_game_audio_ready(void) { return true; }
Sound raylib_lite_game_audio_load_sound(const char *path)
{
    (void)path;
    ++s_loaded;
    return (Sound){.frameCount = 10, .id = s_loaded};
}
void raylib_lite_game_audio_unload_sound(Sound sound) { (void)sound; ++s_unloaded; }
void raylib_lite_game_audio_play_sound(Sound sound) { ++s_played[sound.id]; }
bool raylib_lite_game_audio_is_sound_playing(Sound sound) { (void)sound; return false; }
void raylib_lite_game_audio_set_sound_volume(Sound sound, float volume)
{
    (void)sound;
    assert(volume > 0.0f && volume <= 1.0f);
}
Music raylib_lite_game_audio_load_music(const char *path)
{
    (void)path;
    return (Music){.frameCount = 100, .id = 99};
}
void raylib_lite_game_audio_unload_music(Music music) { (void)music; ++s_unloaded; }
void raylib_lite_game_audio_play_music(Music music) { (void)music; ++s_music_plays; }
void raylib_lite_game_audio_update_music(Music music) { (void)music; }
void raylib_lite_game_audio_stop_music(Music music) { (void)music; ++s_music_stops; }
void raylib_lite_game_audio_set_music_volume(Music music, float volume)
{
    (void)music;
    s_music_volume = volume;
}

static void pulse(uint8_t a, uint16_t b) { (void)a; (void)b; ++s_haptics; }
static void pattern(uint8_t a, uint16_t b, uint8_t c, uint16_t d, uint16_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    ++s_haptics;
}
static void stop(void) { ++s_haptic_stops; }

static unsigned played(unsigned sound) { return s_played[sound + 1U]; }

int main(void)
{
    const shooter_haptics_t haptics = {.pulse = pulse, .pattern = pattern, .stop = stop};
    shooter_game_t game = {0};
    shooter_game_reset(&game, 0x4d4f5341U);
    shooter_feedback_t feedback;
    shooter_feedback_init(&feedback, &game, &haptics);
    assert(s_loaded == SHOOTER_SOUND_COUNT);

    shooter_feedback_update(&feedback, &game, false);
    assert(s_music_plays == 0); /* Title screen is silent until the run starts. */

    game.phase = SHOOTER_PLAYING;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_START) == 1 && s_music_plays == 1 && s_music_volume > .1f);

    ++game.shots_fired;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_SHOT) == 1);
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_SHOT) == 1); /* Deltas are consumed once. */

    /* A kill outranks the shot fired in the same tick. */
    ++game.shots_fired;
    ++game.kills;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_EXPLODE) == 1 && played(SHOOTER_SOUND_SHOT) == 1);

    game.combo = 4;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_POWER) == 1);
    ++game.wave;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_WAVE) == 1);

    --game.lives;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_HURT) == 1);

    game.phase = SHOOTER_PAUSED;
    shooter_feedback_update(&feedback, &game, false);
    assert(s_music_volume < .1f && s_haptic_stops == 1);
    game.phase = SHOOTER_PLAYING;
    shooter_feedback_update(&feedback, &game, false);

    game.lives = 0;
    game.phase = SHOOTER_GAME_OVER;
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_GAME_OVER) == 1 && played(SHOOTER_SOUND_HURT) == 1);
    assert(s_music_stops == 1);

    shooter_game_reset(&game, 0x4d4f5341U);
    shooter_feedback_sync(&feedback, &game);
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_HURT) == 1 && played(SHOOTER_SOUND_START) == 1);

    shooter_game_start(&game);
    shooter_feedback_update(&feedback, &game, false);
    assert(s_music_plays == 2);
    ++game.kills;
    game.phase = SHOOTER_WON;
    shooter_feedback_update(&feedback, &game, false);
    unsigned explosions = played(SHOOTER_SOUND_EXPLODE);
    assert(explosions == 2 && s_music_stops == 2);
    shooter_feedback_update(&feedback, &game, false);
    assert(played(SHOOTER_SOUND_EXPLODE) == explosions && s_music_stops == 2);
    shooter_game_start(&game);
    shooter_feedback_update(&feedback, &game, false);
    assert(s_music_plays == 3); /* Winning and restarting resumes the music once. */

    shooter_feedback_close(&feedback);
    assert(s_unloaded == SHOOTER_SOUND_COUNT + 1 && s_closed == 1);
    shooter_feedback_close(&feedback);
    assert(s_unloaded == SHOOTER_SOUND_COUNT + 1 && s_closed == 1);
    assert(s_haptics > 0);
    puts("shooter feedback: ok");
    return 0;
}
