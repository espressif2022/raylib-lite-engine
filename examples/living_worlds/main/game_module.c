// SPDX-License-Identifier: Apache-2.0
/* One module entry for Host simulation and the loadable ELF. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#if !defined(MOSAICO_GAME_ELF)
#include "host_asset_runtime.h"
#endif
#include "mosaico_game_2d.h"
#if defined(MOSAICO_GAME_ELF)
#include "living_worlds_scene_audio.h"
#endif
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#include "living_worlds_view.h"
#include "living_worlds_world.h"

typedef struct {
    living_world_t world;
    living_worlds_atlases_t atlases;
#if defined(MOSAICO_GAME_ELF)
    living_worlds_scene_audio_t audio;
#endif
    uint8_t loaded_volumes;
    bool paused;
} module_state_t;

static void unload_atlas(MosaicoAtlas *atlas)
{
    if (!atlas || !atlas->texture.id) return;
    UnloadMosaicoAtlas(*atlas);
    *atlas = (MosaicoAtlas){0};
}

static int load_atlas(MosaicoAtlas *atlas, const char *path)
{
    if (atlas->texture.id) return 0;
    *atlas = LoadMosaicoAtlas(path);
    return atlas->texture.id ? 0 : -1;
}

static void unload_volumes(living_worlds_atlases_t *atlases)
{
    unload_atlas(&atlases->sunrise_cliff_front);
    unload_atlas(&atlases->sunrise_cliff_side);
    unload_atlas(&atlases->sunrise_cliff_rear);
    unload_atlas(&atlases->aurora_ice_front);
    unload_atlas(&atlases->aurora_ice_side);
    unload_atlas(&atlases->aurora_ice_rear);
    unload_atlas(&atlases->ocean_left_front);
    unload_atlas(&atlases->ocean_left_side);
    unload_atlas(&atlases->ocean_left_rear);
    unload_atlas(&atlases->ocean_right_front);
    unload_atlas(&atlases->ocean_right_side);
    unload_atlas(&atlases->ocean_right_rear);
    unload_atlas(&atlases->rainforest_falls);
}

static int acquire_volumes(module_state_t *s, uint8_t scene)
{
    if (s->loaded_volumes == scene) return 0;
    unload_volumes(&s->atlases);
    s->loaded_volumes = UINT8_MAX;
    int err = 0;
    if (scene == LIVING_SCENE_AURORA) {
        err = load_atlas(&s->atlases.aurora_ice_front, "aurora_ice_front.atlas") ||
              load_atlas(&s->atlases.aurora_ice_side, "aurora_ice_side.atlas") ||
              load_atlas(&s->atlases.aurora_ice_rear, "aurora_ice_rear.atlas");
    } else if (scene == LIVING_SCENE_SUNRISE) {
        err = load_atlas(&s->atlases.sunrise_cliff_front, "sunrise_cliff_front.atlas") ||
              load_atlas(&s->atlases.sunrise_cliff_side, "sunrise_cliff_side.atlas") ||
              load_atlas(&s->atlases.sunrise_cliff_rear, "sunrise_cliff_rear.atlas");
    } else if (scene == LIVING_SCENE_OCEAN) {
        err = load_atlas(&s->atlases.ocean_left_front, "ocean_reef_left_front.atlas") ||
              load_atlas(&s->atlases.ocean_left_side, "ocean_reef_left_side.atlas") ||
              load_atlas(&s->atlases.ocean_left_rear, "ocean_reef_left_rear.atlas") ||
              load_atlas(&s->atlases.ocean_right_front, "ocean_reef_right_front.atlas") ||
              load_atlas(&s->atlases.ocean_right_side, "ocean_reef_right_side.atlas") ||
              load_atlas(&s->atlases.ocean_right_rear, "ocean_reef_right_rear.atlas");
    } else if (scene == LIVING_SCENE_RAINFOREST) {
        err = load_atlas(&s->atlases.rainforest_falls, "rainforest_falls.atlas");
    }
    if (err) { unload_volumes(&s->atlases); return -1; }
    s->loaded_volumes = scene;
    return 0;
}

static void clear_backgrounds(living_worlds_atlases_t *atlases)
{
#if defined(MOSAICO_GAME_ELF)
    /* The host owns the one decoded photo. These fields alias that texture
     * and must not be unloaded here. */
    atlases->aurora = (MosaicoAtlas){0};
    atlases->ocean = (MosaicoAtlas){0};
    atlases->sunrise = (MosaicoAtlas){0};
    atlases->rainforest = (MosaicoAtlas){0};
#else
    unload_atlas(&atlases->aurora);
    unload_atlas(&atlases->ocean);
    unload_atlas(&atlases->sunrise);
    unload_atlas(&atlases->rainforest);
#endif
}

#if defined(MOSAICO_GAME_ELF)
static const char *background_path(uint8_t scene)
{
    if (scene == LIVING_SCENE_AURORA) return "aurora.jpg";
    if (scene == LIVING_SCENE_SUNRISE) return "sunrise.jpg";
    if (scene == LIVING_SCENE_RAINFOREST) return "rainforest.jpg";
    return "ocean.jpg";
}

static int load_background(module_state_t *s, uint8_t scene)
{
    MosaicoAtlas photo = MosaicoJpegLoad(background_path(scene));
    if (!photo.texture.id) return -1;
    clear_backgrounds(&s->atlases);
    if (scene == LIVING_SCENE_AURORA) s->atlases.aurora = photo;
    else if (scene == LIVING_SCENE_SUNRISE) s->atlases.sunrise = photo;
    else if (scene == LIVING_SCENE_RAINFOREST) s->atlases.rainforest = photo;
    else {
        s->atlases.ocean = photo;
        (void)Mosaico2DCacheTextureLight(photo.texture, 232);
    }
    return 0;
}

#else
static int load_background(module_state_t *s, uint8_t scene)
{
    (void)s;
    (void)scene;
    return 0;
}
#endif

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    module_state_t *s = value;
#if !defined(MOSAICO_GAME_ELF)
    mosaico_host_assets_set_root(asset_root);
    if (load_atlas(&s->atlases.aurora, "aurora.atlas") ||
        load_atlas(&s->atlases.ocean, "ocean.atlas") ||
        load_atlas(&s->atlases.sunrise, "sunrise.atlas") ||
        load_atlas(&s->atlases.rainforest, "rainforest.atlas"))
        return -1;
    (void)Mosaico2DCacheTextureLight(s->atlases.ocean.texture, 232);
#endif
    living_world_reset(&s->world);
    s->loaded_volumes = UINT8_MAX;
    if (load_background(s, s->world.scene) || acquire_volumes(s, s->world.scene)) {
#if defined(MOSAICO_GAME_ELF)
        MosaicoJpegRelease();
#endif
        return -1;
    }
    InitWindow(480, 480, "Living Worlds");
    SetTargetFPS(30);
#if defined(MOSAICO_GAME_ELF)
    living_worlds_scene_audio_init(&s->audio);
#endif
    return 0;
}

static void shutdown(void *value)
{
    module_state_t *s = value;
    if (!s) return;
#if defined(MOSAICO_GAME_ELF)
    living_worlds_scene_audio_close(&s->audio);
#endif
    unload_volumes(&s->atlases);
#if defined(MOSAICO_GAME_ELF)
    MosaicoJpegRelease();
#endif
    clear_backgrounds(&s->atlases);
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    module_state_t *s = value;
    if (!s || !event) return;
    if (event->type == MOSAICO_HOST_INPUT_POINTER) {
        uint8_t previous = s->world.scene;
        living_world_pointer(&s->world, (float)event->x, (float)event->y, event->pressed);
        if (s->world.scene != previous) {
            if (load_background(s, s->world.scene) != 0 ||
                acquire_volumes(s, s->world.scene) != 0) {
                s->world.scene = previous;
                (void)load_background(s, previous);
                (void)acquire_volumes(s, previous);
            } else {
#if defined(MOSAICO_GAME_ELF)
                living_worlds_scene_audio_select(&s->audio, s->world.scene);
#endif
            }
        }
    } else if (event->type == MOSAICO_HOST_INPUT_ACTION && event->pressed) {
        if (event->code == 0) s->world.yaw -= 8;
        else if (event->code == 1) s->world.yaw += 8;
    } else if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        if (event->code == MOSAICO_HOST_CONTROL_PAUSE) s->paused = true;
        else if (event->code == MOSAICO_HOST_CONTROL_RESUME) s->paused = false;
        else if (event->code == MOSAICO_HOST_CONTROL_RESET) {
            living_world_reset(&s->world);
            (void)load_background(s, s->world.scene);
            (void)acquire_volumes(s, s->world.scene);
#if defined(MOSAICO_GAME_ELF)
            living_worlds_scene_audio_select(&s->audio, s->world.scene);
#endif
        }
    }
}

static void update(void *value)
{
    module_state_t *s = value;
    if (!s) return;
    if (!s->paused) living_world_update(&s->world);
#if defined(MOSAICO_GAME_ELF)
    (void)living_worlds_scene_audio_update(&s->audio, s->world.scene);
#endif
}

static int render(void *value)
{
    module_state_t *s = value;
    living_worlds_view_render(&s->world, &s->atlases);
    return 0;
}

static uint32_t state_hash(const void *value)
{
    return living_world_hash(&((const module_state_t *)value)->world);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const living_world_t *w = &((const module_state_t *)value)->world;
#if defined(MOSAICO_GAME_ELF)
    return snprintf(output, capacity,
        "{\"scene\":%u,\"yaw\":%d,\"pitch\":%d,\"dragging\":%s,\"tick\":%lu,"
        "\"state_hash\":\"%08lx\"}",
        w->scene, (int)(w->yaw * 100.0f), (int)(w->pitch * 100.0f),
        w->dragging ? "true" : "false",
        (unsigned long)w->tick, (unsigned long)living_world_hash(w));
#else
    return snprintf(output, capacity,
        "{\"scene\":%u,\"yaw\":%.2f,\"pitch\":%.2f,\"dragging\":%s,\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        w->scene, w->yaw, w->pitch, w->dragging ? "true" : "false",
        (unsigned long)w->tick, (unsigned long)living_world_hash(w));
#endif
}

static const mosaico_game_module_v1_t s_module = {
    .descriptor = {
#if defined(MOSAICO_GAME_ELF)
                   MOSAICO_HOST_GAME_ABI,
#else
                   MOSAICO_HOST_GAME_ABI_V1,
#endif
                   "living_worlds", "Living Worlds",
                   480, 480, 30, 1},
    .state_size = sizeof(module_state_t),
    .initialize = initialize, .shutdown = shutdown, .input = input,
    .update = update, .render = render, .state_hash = state_hash,
    .state_json = state_json,
};

#if defined(MOSAICO_GAME_ELF)
MOSAICO_GAME_MODULE_EXPORT const mosaico_game_module_v1_t *
mosaico_game_module_v1(const mosaico_runtime_v1_t *runtime)
{
    if (runtime) g_mosaico_rt = runtime;
    return &s_module;
}
#else
const mosaico_game_module_v1_t *mosaico_game_module_v1(void)
{
    return &s_module;
}
#endif
