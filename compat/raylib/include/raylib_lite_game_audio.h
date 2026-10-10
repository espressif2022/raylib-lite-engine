// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t mixed_chunks;
    uint32_t write_errors;
    uint32_t underruns;
    uint32_t voice_steals;
    uint8_t active_sfx_voices;
    bool music_playing;
} raylib_lite_game_audio_stats_t;
void raylib_lite_game_audio_init(void);
void raylib_lite_game_audio_close(void);
bool raylib_lite_game_audio_ready(void);
Sound raylib_lite_game_audio_load_sound(const char *asset_path);
void raylib_lite_game_audio_unload_sound(Sound sound);
void raylib_lite_game_audio_play_sound(Sound sound);
void raylib_lite_game_audio_stop_sound(Sound sound);
bool raylib_lite_game_audio_is_sound_playing(Sound sound);
void raylib_lite_game_audio_set_sound_volume(Sound sound,float volume);
Music raylib_lite_game_audio_load_music(const char *asset_path);
void raylib_lite_game_audio_unload_music(Music music);
void raylib_lite_game_audio_play_music(Music music);
void raylib_lite_game_audio_update_music(Music music);
void raylib_lite_game_audio_stop_music(Music music);
void raylib_lite_game_audio_set_music_volume(Music music,float volume);
/* Device master gain, 0..1. Kept across InitAudioDevice. */
void raylib_lite_game_audio_set_master_volume(float volume);
void raylib_lite_game_audio_get_stats(raylib_lite_game_audio_stats_t *stats);
#ifdef __cplusplus
}
#endif
