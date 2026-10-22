// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include "last_zone_feedback.h"

static bool ready;
static unsigned inits, closes, loads, unloads, plays[LAST_ZONE_SOUND_COUNT];
static unsigned music_loads, music_unloads, music_plays, music_stops;
static unsigned music_playing_id;
static float music_volume;
static unsigned haptic_inits, haptic_pulses, haptic_patterns, haptic_stops;

void raylib_lite_game_audio_init(void) { ++inits; }
void raylib_lite_game_audio_close(void) { ++closes; }
bool raylib_lite_game_audio_ready(void) { return ready; }
Sound raylib_lite_game_audio_load_sound(const char *path)
{
    assert(strstr(path, "bolt") == NULL);
    assert(loads < LAST_ZONE_SOUND_COUNT);
    return (Sound){.frameCount = 1, .id = loads++};
}
void raylib_lite_game_audio_unload_sound(Sound sound)
{
    assert(sound.frameCount && sound.id < LAST_ZONE_SOUND_COUNT);
    ++unloads;
}
void raylib_lite_game_audio_play_sound(Sound sound)
{
    assert(sound.frameCount && sound.id < LAST_ZONE_SOUND_COUNT);
    ++plays[sound.id];
}
bool raylib_lite_game_audio_is_sound_playing(Sound sound)
{
    assert(sound.frameCount);
    return false;
}
void raylib_lite_game_audio_set_sound_volume(Sound sound, float volume)
{
    assert(sound.frameCount && volume >= 0.0f && volume <= 1.0f);
}
Music raylib_lite_game_audio_load_music(const char *path)
{
    assert(strcmp(path, "music.sound") == 0 || strcmp(path, "music_combat.sound") == 0);
    ++music_loads;
    return (Music){.frameCount = 1, .id = strstr(path, "combat") ? 2U : 1U};
}
void raylib_lite_game_audio_unload_music(Music music) { assert(music.frameCount); ++music_unloads; }
void raylib_lite_game_audio_play_music(Music music)
{
    assert(music.frameCount);
    ++music_plays;
    music_playing_id = music.id;
}
void raylib_lite_game_audio_update_music(Music music) { assert(music.frameCount); }
void raylib_lite_game_audio_stop_music(Music music) { assert(music.frameCount); ++music_stops; }
void raylib_lite_game_audio_set_music_volume(Music music, float volume)
{
    assert(music.frameCount && volume >= 0.0f && volume <= 1.0f);
    music_volume = volume;
}
static void haptic_init(void) { ++haptic_inits; }
static void haptic_pulse(uint8_t strength, uint16_t duration)
{
    assert(strength && duration);
    ++haptic_pulses;
}
static void haptic_pattern(uint8_t a, uint16_t b, uint8_t c,
                           uint16_t d, uint16_t e)
{
    assert(a && b && c && d && e);
    ++haptic_patterns;
}
static void haptic_stop(void) { ++haptic_stops; }
static void missing_asset(const char *path) { (void)path; assert(false); }

int main(void)
{
    const last_zone_feedback_backend_t backend = {
        .init = haptic_init, .pulse = haptic_pulse,
        .pattern = haptic_pattern, .stop = haptic_stop,
        .missing_asset = missing_asset,
    };
    last_zone_game_t game = {0};
    game.phase = LAST_ZONE_PHASE_PLAYING;
    game.hp = 3;
    game.armor = 2;
    last_zone_feedback_t feedback;
    last_zone_feedback_init(&feedback, &game, &backend);
    assert(inits == 1 && haptic_inits == 1);
    assert(loads == 0 && music_loads == 0);

    ready = true;
    last_zone_feedback_music(&feedback, &game, false);
    assert(loads == LAST_ZONE_SOUND_COUNT && music_loads == LAST_ZONE_MUSIC_COUNT);
    assert(music_plays == 1 && music_playing_id == 1U);
    const float stealth_volume = music_volume;

    /* Contact switches to the combat loop and holds it after contact ends. */
    game.enemies[0].active = true;
    game.enemies[0].ai_state = LAST_ZONE_ENEMY_ENGAGE;
    last_zone_feedback_music(&feedback, &game, false);
    assert(music_plays == 2 && music_stops == 1 && music_playing_id == 2U);
    assert(music_volume > stealth_volume);
    game.enemies[0].ai_state = LAST_ZONE_ENEMY_PATROL;
    for (int i = 0; i < 120; ++i) last_zone_feedback_music(&feedback, &game, false);
    assert(music_playing_id == 2U);
    last_zone_feedback_music(&feedback, &game, true);
    assert(music_volume < .05f);
    for (int i = 0; i < 60; ++i) last_zone_feedback_music(&feedback, &game, false);
    assert(music_playing_id == 1U && music_stops == 2);
    game.enemies[0].active = false;

    game.tick = 1;
    game.last_fire = NEON_FIRE_HIT;
    game.last_pickup = true;
    game.hp = 2;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_RIFLE] == 1);
    assert(plays[LAST_ZONE_SOUND_IMPACT] == 1);
    assert(plays[LAST_ZONE_SOUND_PICKUP] == 1);
    assert(plays[LAST_ZONE_SOUND_HURT] == 1);
    assert(haptic_pulses == 2 && haptic_patterns == 1);
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_RIFLE] == 1);
    assert(plays[LAST_ZONE_SOUND_PICKUP] == 1);
    assert(plays[LAST_ZONE_SOUND_HURT] == 1);

    game.tick = 2;
    game.last_fire = NEON_FIRE_NONE;
    game.last_pickup = false;
    game.hp = 0;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_DEATH] == 1 && plays[LAST_ZONE_SOUND_HURT] == 1);
    game.hp = 2;
    last_zone_feedback_reset(&feedback, &game);

    game.tick = 3;
    game.phase = LAST_ZONE_PHASE_WON;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_EXTRACT] == 1);

    game.tick = 0;
    game.phase = LAST_ZONE_PHASE_START;
    game.hp = 3;
    last_zone_feedback_reset(&feedback, &game);
    game.tick = 1;
    game.phase = LAST_ZONE_PHASE_PLAYING;
    game.last_fire = NEON_FIRE_SHOT;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_RIFLE] == 2 && plays[LAST_ZONE_SOUND_DEPLOY] == 1);
    game.tick = 2;
    game.last_fire = NEON_FIRE_DOOR;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_DOOR] == 1);
    game.tick = 3;
    game.last_fire = NEON_FIRE_NONE;
    game.last_miss = true;
    game.last_radio = true;
    last_zone_feedback_events(&feedback, &game);
    assert(plays[LAST_ZONE_SOUND_WHIZZ] == 1 && plays[LAST_ZONE_SOUND_RADIO] == 1);

    last_zone_feedback_close(&feedback);
    assert(music_stops == 3 && music_unloads == LAST_ZONE_MUSIC_COUNT);
    assert(unloads == LAST_ZONE_SOUND_COUNT && closes == 1 && haptic_stops == 1);
    return 0;
}
