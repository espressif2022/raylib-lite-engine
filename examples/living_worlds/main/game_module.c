// SPDX-License-Identifier: Apache-2.0
/* Host and ELF adapters for the shared Living Worlds session. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#if !defined(MOSAICO_GAME_ELF)
#include "host_asset_runtime.h"
#endif
#include "mosaico_game_module.h"
#include "raylib_lite_raylib.h"
#include "living_worlds_session.h"

typedef living_worlds_session_t module_state_t;

static void unload_atlas(raylib_lite_atlas_t *atlas)
{
    if (!atlas->texture.id) return;
    raylib_lite_atlas_unload(*atlas);
    *atlas = (raylib_lite_atlas_t){0};
}

static int load_atlas(raylib_lite_atlas_t *atlas, const char *path)
{
    *atlas = raylib_lite_atlas_load(path);
    return atlas->texture.id ? 0 : -1;
}

static void clear_backgrounds(living_worlds_atlases_t *atlases)
{
#if defined(MOSAICO_GAME_ELF)
    /* The runtime owns the decoded JPEG texture. */
    atlases->aurora = (raylib_lite_atlas_t){0};
    atlases->ocean = (raylib_lite_atlas_t){0};
    atlases->sunrise = (raylib_lite_atlas_t){0};
    atlases->rainforest = (raylib_lite_atlas_t){0};
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
#endif

static int load_background(void *context, living_worlds_atlases_t *atlases,
                           uint8_t scene)
{
    (void)context;
#if defined(MOSAICO_GAME_ELF)
    raylib_lite_atlas_t photo = MosaicoJpegLoad(background_path(scene));
    if (!photo.texture.id) return -1;
    clear_backgrounds(atlases);
    if (scene == LIVING_SCENE_AURORA) atlases->aurora = photo;
    else if (scene == LIVING_SCENE_SUNRISE) atlases->sunrise = photo;
    else if (scene == LIVING_SCENE_RAINFOREST) atlases->rainforest = photo;
    else {
        atlases->ocean = photo;
        (void)raylib_lite_2d_cache_texture_light(photo.texture, 232);
    }
#else
    (void)atlases;
    (void)scene;
#endif
    return 0;
}

static void release_background(void *context, living_worlds_atlases_t *atlases)
{
    (void)context;
#if defined(MOSAICO_GAME_ELF)
    MosaicoJpegRelease();
#endif
    clear_backgrounds(atlases);
}

static const living_worlds_session_assets_t s_assets = {
    .load_background = load_background,
    .release_background = release_background,
};

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
        load_atlas(&s->atlases.rainforest, "rainforest.atlas")) {
        clear_backgrounds(&s->atlases);
        return -1;
    }
    (void)raylib_lite_2d_cache_texture_light(s->atlases.ocean.texture, 232);
#endif
    if (living_worlds_session_start(s, &s_assets, s) != 0) return -1;
    InitWindow(480, 480, "Living Worlds");
    SetTargetFPS(30);
    return 0;
}

static void shutdown(void *value)
{
    living_worlds_session_close(value);
}

static void input(void *value, const mosaico_host_input_v1_t *event)
{
    module_state_t *s = value;
    if (!s || !event) return;
    if (event->type == MOSAICO_HOST_INPUT_POINTER)
        living_worlds_session_pointer(s, (float)event->x, (float)event->y,
                                      event->pressed);
    else if (event->type == MOSAICO_HOST_INPUT_ACTION)
        living_worlds_session_action(s, event->code, event->pressed);
    else if (event->type == MOSAICO_HOST_INPUT_CONTROL) {
        if (event->code == MOSAICO_HOST_CONTROL_PAUSE) s->paused = true;
        else if (event->code == MOSAICO_HOST_CONTROL_RESUME) s->paused = false;
        else if (event->code == MOSAICO_HOST_CONTROL_RESET)
            living_worlds_session_reset(s);
    }
}

static void update(void *value)
{
    module_state_t *s = value;
    if (s) (void)living_worlds_session_update(s);
}

static int render(void *value)
{
    living_worlds_session_render(value);
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
