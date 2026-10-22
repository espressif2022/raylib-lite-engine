// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "rally_feedback.h"

static unsigned s_loaded, s_unloaded, s_closed, s_haptic_stops;
static unsigned s_pulses, s_patterns;
static int s_strength, s_first_ms, s_second_strength, s_gap_ms, s_second_ms;
static unsigned s_played[RALLY_SOUND_COUNT + 1];
static unsigned s_music_played[4], s_music_stopped[4];
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
    unsigned id = strstr(path, "neon") ? 1U : strstr(path, "sunset") ? 2U : 3U;
    return (Music){.frameCount = 1000, .id = id};
}
void raylib_lite_game_audio_unload_music(Music music) { (void)music; ++s_unloaded; }
void raylib_lite_game_audio_play_music(Music music) { ++s_music_played[music.id]; }
void raylib_lite_game_audio_update_music(Music music) { (void)music; }
void raylib_lite_game_audio_stop_music(Music music) { ++s_music_stopped[music.id]; }
void raylib_lite_game_audio_set_music_volume(Music music, float volume)
{
    s_music_volume[music.id] = volume;
}

static void pulse(uint8_t strength, uint16_t ms)
{
    ++s_pulses;
    s_strength = strength;
    s_first_ms = ms;
}
static void pattern(uint8_t a, uint16_t b, uint8_t c, uint16_t d, uint16_t e)
{
    ++s_patterns;
    s_strength = a;
    s_first_ms = b;
    s_second_strength = c;
    s_gap_ms = d;
    s_second_ms = e;
}
static void haptic_stop(void) { ++s_haptic_stops; }

/* Sound ids follow load order, which follows the RALLY_SOUND_* enum. */
static unsigned played(unsigned sound) { return s_played[sound + 1U]; }

static void fire(rally_feedback_t *feedback, rally_game_t *game, uint32_t events)
{
    ++game->event_serial;
    game->event_flags = events;
    rally_feedback_events(feedback, game);
}

int main(void)
{
    const rally_haptics_t haptics = {
        .pulse = pulse, .pattern = pattern, .stop = haptic_stop};
    rally_feedback_t feedback;
    rally_game_t game;
    rally_reset(&game);
    rally_feedback_init(&feedback, &haptics);
    assert(s_loaded == RALLY_SOUND_COUNT);

    /* Countdown: one beep per displayed digit, then GO on the start event. */
    for (unsigned i = 0; i < 90 && game.phase == RALLY_PHASE_COUNTDOWN; ++i) {
        rally_update(&game);
        rally_feedback_events(&feedback, &game);
        rally_feedback_update(&feedback, &game, 0, false);
    }
    assert(game.phase == RALLY_PHASE_PLAYING);
    assert(played(RALLY_SOUND_COUNTDOWN) == 3);
    assert(played(RALLY_SOUND_GO) == 1);
    assert(s_music_played[1] == 1 && s_music_volume[1] > .1f);

    /* Collision outranks the near miss in the same tick; pattern order is native. */
    const unsigned patterns = s_patterns;
    fire(&feedback, &game, RALLY_EVENT_COLLISION | RALLY_EVENT_NEAR_MISS);
    assert(s_patterns == patterns + 1 && s_strength == 100 && s_first_ms == 55);
    assert(s_second_strength == 82 && s_gap_ms == 46 && s_second_ms == 28);
    assert(played(RALLY_SOUND_COLLISION) == 1 && played(RALLY_SOUND_NEAR_MISS) == 0);
    rally_feedback_events(&feedback, &game);
    assert(played(RALLY_SOUND_COLLISION) == 1); /* Same serial is consumed once. */

    fire(&feedback, &game, RALLY_EVENT_OVERTAKE | RALLY_EVENT_CHECKPOINT);
    assert(played(RALLY_SOUND_OVERTAKE) == 1 && s_strength == 46);
    fire(&feedback, &game, RALLY_EVENT_OVERTAKEN);
    assert(played(RALLY_SOUND_OVERTAKEN) == 1);
    fire(&feedback, &game, RALLY_EVENT_NEAR_MISS);
    assert(played(RALLY_SOUND_NEAR_MISS) == 1);

    /* Low-priority cues share a four-tick cooldown. */
    fire(&feedback, &game, RALLY_EVENT_DRAFT_READY);
    assert(played(RALLY_SOUND_DRAFT) == 1 && feedback.cooldown == 4);
    fire(&feedback, &game, RALLY_EVENT_CHECKPOINT);
    assert(played(RALLY_SOUND_CHECKPOINT) == 0);
    for (unsigned i = 0; i < 4; ++i) rally_feedback_update(&feedback, &game, 0, false);
    fire(&feedback, &game, RALLY_EVENT_CHECKPOINT);
    assert(played(RALLY_SOUND_CHECKPOINT) == 1);
    for (unsigned i = 0; i < 4; ++i) rally_feedback_update(&feedback, &game, 0, false);
    fire(&feedback, &game, RALLY_EVENT_RIVAL_BOOST);
    assert(played(RALLY_SOUND_RIVAL) == 1 && s_strength == 16);
    for (unsigned i = 0; i < 4; ++i) rally_feedback_update(&feedback, &game, 0, false);
    fire(&feedback, &game, RALLY_EVENT_RIVAL_SLIP);
    assert(played(RALLY_SOUND_RIVAL) == 1 && s_strength == 22);

    /* Music follows speed, ducks while paused and switches with the course. */
    game.speed = 0.0f;
    rally_feedback_update(&feedback, &game, 0, false);
    const float slow = s_music_volume[1];
    game.speed = RALLY_MAX_SPEED;
    rally_feedback_update(&feedback, &game, 0, false);
    assert(s_music_volume[1] > slow);
    rally_feedback_pause(&feedback);
    assert(s_haptic_stops == 1);
    rally_feedback_update(&feedback, &game, 0, true);
    assert(s_music_volume[1] < .05f);
    rally_feedback_update(&feedback, &game, 2, false);
    assert(s_music_stopped[1] == 1 && s_music_played[3] == 1);

    game.phase = RALLY_PHASE_FINISHED;
    fire(&feedback, &game, RALLY_EVENT_FINISH | RALLY_EVENT_LAP);
    rally_feedback_update(&feedback, &game, 2, false);
    assert(played(RALLY_SOUND_FINISH) == 1 && played(RALLY_SOUND_LAP) == 0);
    assert(s_music_stopped[3] == 1 && feedback.playing_track == -1);

    rally_feedback_reset(&feedback);
    assert(s_haptic_stops == 2 && feedback.last_serial == 0);
    rally_feedback_close(&feedback);
    assert(s_unloaded == RALLY_SOUND_COUNT + RALLY_MUSIC_COUNT && s_closed == 1);
    assert(s_haptic_stops == 3);
    (void)s_pulses;
    puts("rally feedback: ok");
    return 0;
}
