// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include "living_worlds_scene_audio.h"

static bool ready;
static unsigned inits, closes, loads, unloads, plays, stops, updates, volumes;
static unsigned last_play, last_stop, last_update;

void MosaicoAudioInit(void) { ++inits; }
void MosaicoAudioClose(void) { ++closes; }
bool MosaicoAudioReady(void) { return ready; }
Music MosaicoAudioLoadMusic(const char *path)
{
    for (unsigned i = 0; i < SCENE_AUDIO_COUNT; ++i) {
        if (strcmp(path, SCENE_AUDIO_PATHS[i]) == 0) {
            ++loads;
            return (Music){.frameCount = 1, .id = i};
        }
    }
    assert(false);
    return (Music){0};
}
void MosaicoAudioUnloadMusic(Music music) { assert(music.frameCount); ++unloads; }
void MosaicoAudioPlayMusic(Music music) { ++plays; last_play = music.id; }
void MosaicoAudioUpdateMusic(Music music) { ++updates; last_update = music.id; }
void MosaicoAudioStopMusic(Music music) { ++stops; last_stop = music.id; }
void MosaicoAudioSetMusicVolume(Music music, float volume)
{
    assert(music.frameCount && volume > 0.64f && volume < 0.66f);
    ++volumes;
}

int main(void)
{
    living_worlds_scene_audio_t audio = {0};
    living_worlds_scene_audio_init(&audio);
    assert(inits == 1 && audio.playing_scene == UINT8_MAX);
    assert(!living_worlds_scene_audio_update(&audio, LIVING_SCENE_OCEAN));
    assert(loads == 0 && plays == 0);

    ready = true;
    assert(living_worlds_scene_audio_update(&audio, LIVING_SCENE_OCEAN));
    assert(loads == SCENE_AUDIO_COUNT && plays == 1 && volumes == 1);
    assert(last_play == LIVING_SCENE_OCEAN && last_update == LIVING_SCENE_OCEAN);
    assert(!living_worlds_scene_audio_update(&audio, LIVING_SCENE_OCEAN));
    assert(loads == SCENE_AUDIO_COUNT && plays == 1 && updates == 2);

    living_worlds_scene_audio_select(&audio, LIVING_SCENE_AURORA);
    assert(stops == 1 && last_stop == LIVING_SCENE_OCEAN);
    assert(plays == 2 && last_play == LIVING_SCENE_AURORA);
    living_worlds_scene_audio_select(&audio, LIVING_SCENE_AURORA);
    assert(stops == 1 && plays == 2);
    living_worlds_scene_audio_select(&audio, LIVING_SCENE_RAINFOREST);
    assert(stops == 2 && last_stop == LIVING_SCENE_AURORA);
    assert(plays == 3 && last_play == LIVING_SCENE_RAINFOREST);

    living_worlds_scene_audio_close(&audio);
    assert(stops == 3 && last_stop == LIVING_SCENE_RAINFOREST);
    assert(unloads == SCENE_AUDIO_COUNT && closes == 1);
    assert(audio.playing_scene == UINT8_MAX && !audio.load_attempted);
    return 0;
}
