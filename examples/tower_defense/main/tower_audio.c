// SPDX-License-Identifier: Apache-2.0
#include "tower_audio.h"
#include <stdbool.h>
#include "raylib_lite_raylib_audio.h"
static Sound s_cues[TOWER_AUDIO_GAME_OVER+1];
static Music s_music;
static bool s_music_stopped;
static const char *const CUE_PATHS[]={"wave.sound","build.sound","shot.sound",
    "explosion.sound","wave.sound","leak.sound","game_over.sound"};

raylib_lite_result_t tower_audio_init(void)
{
    InitAudioDevice();
    if(!IsAudioDeviceReady())return RAYLIB_LITE_INVALID_STATE;
    for(unsigned i=0;i<=TOWER_AUDIO_GAME_OVER;++i)s_cues[i]=LoadSound(CUE_PATHS[i]);
    s_music=LoadMusicStream("music.sound");
    SetMusicVolume(s_music,.24f);PlayMusicStream(s_music);s_music_stopped=false;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t tower_audio_play(tower_audio_cue_t cue)
{
    if(cue>TOWER_AUDIO_GAME_OVER||!s_cues[cue].frameCount)return RAYLIB_LITE_INVALID_STATE;
    PlaySound(s_cues[cue]);return RAYLIB_LITE_OK;
}

void tower_audio_set_music(bool paused, bool game_over)
{
    if(!IsAudioDeviceReady())return;
    if(game_over){if(!s_music_stopped)StopMusicStream(s_music);s_music_stopped=true;return;}
    SetMusicVolume(s_music,paused?.07f:.24f);
    if(s_music_stopped){PlayMusicStream(s_music);s_music_stopped=false;}
}

void tower_audio_shutdown(void)
{
    if(!IsAudioDeviceReady())return;
    StopMusicStream(s_music);UnloadMusicStream(s_music);
    for(unsigned i=0;i<=TOWER_AUDIO_GAME_OVER;++i)
        if(s_cues[i].frameCount){UnloadSound(s_cues[i]);s_cues[i]=(Sound){0};}
    s_music=(Music){0};s_music_stopped=true;
    CloseAudioDevice();
}
