// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <string.h>
#include "living_worlds_session.h"

static bool fail_aurora;
static unsigned atlas_loads, atlas_unloads, background_loads, background_releases;
static unsigned audio_inits, audio_closes, music_loads, music_unloads;
static unsigned music_plays[SCENE_AUDIO_COUNT], music_stops, music_updates;

MosaicoAtlas LoadMosaicoAtlas(const char *path)
{
    ++atlas_loads;
    if (fail_aurora && strcmp(path, "aurora_ice_side.atlas") == 0)
        return (MosaicoAtlas){0};
    return (MosaicoAtlas){.texture.id = atlas_loads};
}
void UnloadMosaicoAtlas(MosaicoAtlas atlas)
{
    assert(atlas.texture.id);
    ++atlas_unloads;
}
void living_world_reset(living_world_t *world)
{
    world->scene = LIVING_SCENE_OCEAN;
    world->tick = 0;
}
void living_world_pointer(living_world_t *world, float x, float y, bool pressed)
{
    (void)y;
    if (pressed) world->scene = x < 100.0f ? LIVING_SCENE_AURORA
                                          : LIVING_SCENE_OCEAN;
}
void living_world_update(living_world_t *world) { ++world->tick; }
void living_worlds_view_render(const living_world_t *world,
                               const living_worlds_atlases_t *atlases)
{
    assert(world && atlases);
}
void MosaicoAudioInit(void) { ++audio_inits; }
void MosaicoAudioClose(void) { ++audio_closes; }
bool MosaicoAudioReady(void) { return true; }
Music MosaicoAudioLoadMusic(const char *path)
{
    assert(path && music_loads < SCENE_AUDIO_COUNT);
    return (Music){.frameCount = 1, .id = music_loads++};
}
void MosaicoAudioUnloadMusic(Music music)
{
    assert(music.frameCount);
    ++music_unloads;
}
void MosaicoAudioPlayMusic(Music music)
{
    assert(music.frameCount && music.id < SCENE_AUDIO_COUNT);
    ++music_plays[music.id];
}
void MosaicoAudioUpdateMusic(Music music)
{
    assert(music.frameCount);
    ++music_updates;
}
void MosaicoAudioStopMusic(Music music)
{
    assert(music.frameCount);
    ++music_stops;
}
void MosaicoAudioSetMusicVolume(Music music, float volume)
{
    assert(music.frameCount && volume > 0.0f && volume < 1.0f);
}
static int load_background(void *context, living_worlds_atlases_t *atlases,
                           uint8_t scene)
{
    assert(context && atlases && scene < SCENE_AUDIO_COUNT);
    ++background_loads;
    return 0;
}
static void release_background(void *context, living_worlds_atlases_t *atlases)
{
    assert(context && atlases);
    ++background_releases;
}

int main(void)
{
    const living_worlds_session_assets_t assets = {
        .load_background = load_background,
        .release_background = release_background,
    };
    living_worlds_session_t session = {0};
    assert(living_worlds_session_start(&session, &assets, &session) == 0);
    assert(session.world.scene == LIVING_SCENE_OCEAN);
    assert(atlas_loads == 6 && audio_inits == 1);
    assert(living_worlds_session_update(&session));
    assert(music_loads == SCENE_AUDIO_COUNT);
    assert(music_plays[LIVING_SCENE_OCEAN] == 1);

    fail_aurora = true;
    living_worlds_session_pointer(&session, 0, 0, true);
    assert(session.world.scene == LIVING_SCENE_OCEAN);
    assert(session.loaded_volumes == LIVING_SCENE_OCEAN);
    assert(background_loads == 3);
    assert(music_stops == 0 && music_plays[LIVING_SCENE_AURORA] == 0);

    fail_aurora = false;
    living_worlds_session_pointer(&session, 0, 0, true);
    assert(session.world.scene == LIVING_SCENE_AURORA);
    assert(music_stops == 1 && music_plays[LIVING_SCENE_AURORA] == 1);
    session.paused = true;
    assert(!living_worlds_session_update(&session));
    assert(session.world.tick == 1 && music_updates == 2);
    living_worlds_session_reset(&session);
    assert(session.world.scene == LIVING_SCENE_OCEAN);
    assert(music_plays[LIVING_SCENE_OCEAN] == 2);

    living_worlds_session_render(&session);
    living_worlds_session_close(&session);
    assert(background_releases == 1 && music_stops == 3);
    assert(music_unloads == SCENE_AUDIO_COUNT && audio_closes == 1);
    living_worlds_session_close(&session);
    assert(background_releases == 1 && audio_closes == 1);
    assert(atlas_unloads > 0);
    return 0;
}
