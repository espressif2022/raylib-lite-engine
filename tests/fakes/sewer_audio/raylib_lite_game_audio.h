#pragma once
#include <stdbool.h>
typedef struct { unsigned frameCount; } Sound;
void raylib_lite_game_audio_init(void);
bool raylib_lite_game_audio_ready(void);
Sound raylib_lite_game_audio_load_sound(const char *path);
void raylib_lite_game_audio_set_sound_volume(Sound sound,float volume);
void raylib_lite_game_audio_play_sound(Sound sound);
void raylib_lite_game_audio_stop_sound(Sound sound);
void raylib_lite_game_audio_unload_sound(Sound sound);
void raylib_lite_game_audio_close(void);
