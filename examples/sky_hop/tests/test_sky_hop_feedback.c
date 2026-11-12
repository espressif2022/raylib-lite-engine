// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sky_hop_feedback.h"

static unsigned s_loaded, s_unloaded, s_closed, s_pulses, s_haptic_stops;
static unsigned s_played[32], s_music_played[4], s_music_stopped[4];
static float s_music_volume[4];

void raylib_lite_game_audio_init(void) {}
void raylib_lite_game_audio_close(void) { ++s_closed; }
bool raylib_lite_game_audio_ready(void) { return true; }
Sound raylib_lite_game_audio_load_sound(const char *path)
{
    (void)path;
    ++s_loaded;
    return (Sound){.frameCount = 100, .id = s_loaded};
}
void raylib_lite_game_audio_unload_sound(Sound sound) { (void)sound; ++s_unloaded; }
void raylib_lite_game_audio_play_sound(Sound sound) { ++s_played[sound.id]; }
bool raylib_lite_game_audio_is_sound_playing(Sound sound) { (void)sound; return false; }
void raylib_lite_game_audio_set_sound_volume(Sound sound, float volume)
{
    (void)sound;
    (void)volume;
}
Music raylib_lite_game_audio_load_music(const char *path)
{
    return (Music){.frameCount = 1000, .id = strstr(path, "night") ? 2U : 1U};
}
void raylib_lite_game_audio_unload_music(Music music) { (void)music; ++s_unloaded; }
void raylib_lite_game_audio_play_music(Music music) { ++s_music_played[music.id]; }
void raylib_lite_game_audio_update_music(Music music) { (void)music; }
void raylib_lite_game_audio_stop_music(Music music) { ++s_music_stopped[music.id]; }
void raylib_lite_game_audio_set_music_volume(Music music, float volume)
{
    s_music_volume[music.id] = volume;
}

static void pulse(uint8_t strength, uint16_t ms) { (void)strength; (void)ms; ++s_pulses; }
static void haptic_pattern(uint8_t a, uint16_t b, uint8_t c, uint16_t d, uint16_t e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    ++s_pulses;
}
static void haptic_stop(void) { ++s_haptic_stops; }

static unsigned total_played(void)
{
    unsigned total = 0;
    for (unsigned i = 0; i < 32; ++i) total += s_played[i];
    return total;
}

int main(void)
{
    const sky_hop_haptics_t haptics = {
        .pulse = pulse, .pattern = haptic_pattern, .stop = haptic_stop};
    sky_hop_feedback_t feedback;
    platform_game_t game;
    platform_game_reset(&game);
    sky_hop_feedback_init(&feedback, &haptics);
    assert(s_loaded == SKY_HOP_SOUND_COUNT);

    /* Title screen is silent; level 1 uses the day loop. */
    sky_hop_feedback_music(&feedback, &game, false);
    assert(feedback.playing_track == -1 && s_music_played[1] == 0);
    platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
    sky_hop_feedback_music(&feedback, &game, false);
    assert(feedback.playing_track == SKY_HOP_MUSIC_DAY && s_music_played[1] == 1);
    sky_hop_feedback_music(&feedback, &game, false);
    assert(s_music_played[1] == 1);

    /* Each consumed event set plays exactly one cue. */
    uint32_t events = platform_game_take_events(&game);
    sky_hop_feedback_events(&feedback, &game, events);
    assert(total_played() == 1 && s_played[SKY_HOP_SOUND_START + 1] == 1);
    sky_hop_feedback_events(&feedback, &game, platform_game_take_events(&game));
    assert(total_played() == 1);
    sky_hop_feedback_events(&feedback, &game,
                            PLATFORM_EVENT_COIN | PLATFORM_EVENT_JUMP | PLATFORM_EVENT_HURT);
    assert(total_played() == 2 && s_played[SKY_HOP_SOUND_HURT + 1] == 1);

    /* Pausing ducks the music and stops haptics; level 2 swaps to the night loop. */
    platform_game_set_action(&game, PLATFORM_ACTION_PAUSE, true);
    sky_hop_feedback_events(&feedback, &game, platform_game_take_events(&game));
    sky_hop_feedback_music(&feedback, &game, false);
    assert(s_haptic_stops == 1 && s_music_volume[1] < .1f);
    platform_game_set_action(&game, PLATFORM_ACTION_PAUSE, true);
    game.player_x = platform_game_finish_x(&game) + 1;
    platform_game_update(&game);
    assert(game.phase == PLATFORM_LEVEL_CLEAR);
    sky_hop_feedback_music(&feedback, &game, false);
    assert(feedback.playing_track == -1 && s_music_stopped[1] == 1);
    platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
    sky_hop_feedback_music(&feedback, &game, false);
    assert(feedback.playing_track == SKY_HOP_MUSIC_NIGHT && s_music_played[2] == 1);
    sky_hop_feedback_music(&feedback, &game, true);
    assert(feedback.playing_track == -1 && s_music_stopped[2] == 1);

    sky_hop_feedback_close(&feedback);
    assert(s_unloaded == SKY_HOP_SOUND_COUNT + SKY_HOP_MUSIC_COUNT && s_closed == 1);
    assert(s_pulses > 0);
    puts("sky hop feedback: ok");
    return 0;
}
