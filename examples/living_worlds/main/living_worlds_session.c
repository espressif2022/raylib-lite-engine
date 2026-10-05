// SPDX-License-Identifier: Apache-2.0
#include "living_worlds_session.h"
#include <stddef.h>

static void unload_atlas(raylib_lite_atlas_t *atlas)
{
    if (!atlas->texture.id) return;
    raylib_lite_atlas_unload(*atlas);
    *atlas = (raylib_lite_atlas_t){0};
}

static void unload_volumes(living_worlds_atlases_t *a)
{
    unload_atlas(&a->sunrise_cliff_front);
    unload_atlas(&a->sunrise_cliff_side);
    unload_atlas(&a->sunrise_cliff_rear);
    unload_atlas(&a->aurora_ice_front);
    unload_atlas(&a->aurora_ice_side);
    unload_atlas(&a->aurora_ice_rear);
    unload_atlas(&a->ocean_left_front);
    unload_atlas(&a->ocean_left_side);
    unload_atlas(&a->ocean_left_rear);
    unload_atlas(&a->ocean_right_front);
    unload_atlas(&a->ocean_right_side);
    unload_atlas(&a->ocean_right_rear);
    unload_atlas(&a->rainforest_falls);
}

static int load_atlas(raylib_lite_atlas_t *atlas, const char *path)
{
    if (atlas->texture.id) return 0;
    *atlas = raylib_lite_atlas_load(path);
    return atlas->texture.id ? 0 : -1;
}

static int load_volumes(living_worlds_session_t *session, uint8_t scene)
{
    if (session->loaded_volumes == scene) return 0;
    living_worlds_atlases_t *a = &session->atlases;
    unload_volumes(a);
    session->loaded_volumes = UINT8_MAX;
    int error = 0;
    if (scene == LIVING_SCENE_AURORA) {
        error = load_atlas(&a->aurora_ice_front, "aurora_ice_front.atlas") ||
                load_atlas(&a->aurora_ice_side, "aurora_ice_side.atlas") ||
                load_atlas(&a->aurora_ice_rear, "aurora_ice_rear.atlas");
    } else if (scene == LIVING_SCENE_SUNRISE) {
        error = load_atlas(&a->sunrise_cliff_front, "sunrise_cliff_front.atlas") ||
                load_atlas(&a->sunrise_cliff_side, "sunrise_cliff_side.atlas") ||
                load_atlas(&a->sunrise_cliff_rear, "sunrise_cliff_rear.atlas");
    } else if (scene == LIVING_SCENE_OCEAN) {
        error = load_atlas(&a->ocean_left_front, "ocean_reef_left_front.atlas") ||
                load_atlas(&a->ocean_left_side, "ocean_reef_left_side.atlas") ||
                load_atlas(&a->ocean_left_rear, "ocean_reef_left_rear.atlas") ||
                load_atlas(&a->ocean_right_front, "ocean_reef_right_front.atlas") ||
                load_atlas(&a->ocean_right_side, "ocean_reef_right_side.atlas") ||
                load_atlas(&a->ocean_right_rear, "ocean_reef_right_rear.atlas");
    } else if (scene == LIVING_SCENE_RAINFOREST) {
        error = load_atlas(&a->rainforest_falls, "rainforest_falls.atlas");
    }
    if (error) {
        unload_volumes(a);
        return -1;
    }
    session->loaded_volumes = scene;
    return 0;
}

static int load_background(living_worlds_session_t *session, uint8_t scene)
{
    return session->assets->load_background(session->asset_context,
                                             &session->atlases, scene);
}

int living_worlds_session_start(living_worlds_session_t *session,
                                const living_worlds_session_assets_t *assets,
                                void *asset_context)
{
    if (!session || !assets || !assets->load_background ||
        !assets->release_background) return -1;
    session->assets = assets;
    session->asset_context = asset_context;
    session->loaded_volumes = UINT8_MAX;
    living_world_reset(&session->world);
    if (load_background(session, session->world.scene) ||
        load_volumes(session, session->world.scene)) {
        living_worlds_session_close(session);
        return -1;
    }
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    living_worlds_scene_audio_init(&session->audio);
    session->audio_started = true;
#endif
    return 0;
}

void living_worlds_session_pointer(living_worlds_session_t *session,
                                   float x, float y, bool pressed)
{
    uint8_t previous = session->world.scene;
    living_world_pointer(&session->world, x, y, pressed);
    if (session->world.scene == previous) return;
    if (load_background(session, session->world.scene) ||
        load_volumes(session, session->world.scene)) {
        session->world.scene = previous;
        (void)load_background(session, previous);
        (void)load_volumes(session, previous);
        return;
    }
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    living_worlds_scene_audio_select(&session->audio, session->world.scene);
#endif
}

void living_worlds_session_action(living_worlds_session_t *session,
                                  int code, bool pressed)
{
    if (!pressed) return;
    if (code == 0) session->world.yaw -= 8;
    else if (code == 1) session->world.yaw += 8;
}

void living_worlds_session_reset(living_worlds_session_t *session)
{
    living_world_reset(&session->world);
    (void)load_background(session, session->world.scene);
    (void)load_volumes(session, session->world.scene);
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    living_worlds_scene_audio_select(&session->audio, session->world.scene);
#endif
}

bool living_worlds_session_update(living_worlds_session_t *session)
{
    if (!session->paused) living_world_update(&session->world);
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    return living_worlds_scene_audio_update(&session->audio,
                                            session->world.scene);
#else
    return false;
#endif
}

void living_worlds_session_render(living_worlds_session_t *session)
{
    living_worlds_view_render(&session->world, &session->atlases);
}

void living_worlds_session_close(living_worlds_session_t *session)
{
    if (!session || !session->assets) return;
#if defined(MOSAICO_GAME_ELF) || defined(MOSAICO_GAME_NATIVE)
    if (session->audio_started) {
        living_worlds_scene_audio_close(&session->audio);
        session->audio_started = false;
    }
#endif
    unload_volumes(&session->atlases);
    session->assets->release_background(session->asset_context,
                                        &session->atlases);
    session->loaded_volumes = UINT8_MAX;
    session->assets = NULL;
    session->asset_context = NULL;
}
