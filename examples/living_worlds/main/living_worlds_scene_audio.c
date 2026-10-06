// SPDX-License-Identifier: Apache-2.0
#include "living_worlds_scene_audio.h"

const char *const SCENE_AUDIO_PATHS[SCENE_AUDIO_COUNT] = {
    [LIVING_SCENE_AURORA] = "aurora_wind_ice.sound",
    [LIVING_SCENE_OCEAN] = "ocean_ambience.sound",
    [LIVING_SCENE_SUNRISE] = "sunrise_wind.sound",
    [LIVING_SCENE_RAINFOREST] = "rainforest_ambience.sound",
};

void living_worlds_scene_audio_init(living_worlds_scene_audio_t *audio)
{
    *audio = (living_worlds_scene_audio_t){.playing_scene = UINT8_MAX};
    raylib_lite_game_audio_init();
}

void living_worlds_scene_audio_select(living_worlds_scene_audio_t *audio,
                                      uint8_t scene)
{
    if (audio->playing_scene == scene) return;
    if (audio->playing_scene < SCENE_AUDIO_COUNT)
        raylib_lite_game_audio_stop_music(audio->tracks[audio->playing_scene]);
    audio->playing_scene = UINT8_MAX;
    if (scene >= SCENE_AUDIO_COUNT || !audio->tracks[scene].frameCount) return;
    raylib_lite_game_audio_set_music_volume(audio->tracks[scene], 0.65f);
    raylib_lite_game_audio_play_music(audio->tracks[scene]);
    audio->playing_scene = scene;
}

bool living_worlds_scene_audio_update(living_worlds_scene_audio_t *audio,
                                      uint8_t scene)
{
    bool loaded = false;
    if (!audio->load_attempted && raylib_lite_game_audio_ready()) {
        audio->load_attempted = true;
        loaded = true;
        for (unsigned i = 0; i < SCENE_AUDIO_COUNT; ++i)
            audio->tracks[i] = raylib_lite_game_audio_load_music(SCENE_AUDIO_PATHS[i]);
        living_worlds_scene_audio_select(audio, scene);
    }
    if (audio->playing_scene < SCENE_AUDIO_COUNT)
        raylib_lite_game_audio_update_music(audio->tracks[audio->playing_scene]);
    return loaded;
}

void living_worlds_scene_audio_close(living_worlds_scene_audio_t *audio)
{
    living_worlds_scene_audio_select(audio, UINT8_MAX);
    for (unsigned i = 0; i < SCENE_AUDIO_COUNT; ++i) {
        if (!audio->tracks[i].frameCount) continue;
        raylib_lite_game_audio_unload_music(audio->tracks[i]);
        audio->tracks[i] = (Music){0};
    }
    raylib_lite_game_audio_close();
    audio->load_attempted = false;
}
