// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_module_contract.h"
/* Host, native and ELF adapters for the shared Living Worlds session. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#if !defined(MOSAICO_GAME_ELF) && !defined(RAYLIB_LITE_GAME_NATIVE)
#include "host_asset_runtime.h"
#endif
#if defined(RAYLIB_LITE_GAME_NATIVE)
#include "living_worlds_native_assets.h"
#endif
#include "raylib_lite_raylib.h"
#include "living_worlds_session.h"

typedef struct {
    living_worlds_session_t session;
#if defined(RAYLIB_LITE_GAME_NATIVE)
    living_worlds_native_assets_t *native_assets;
#endif
} module_state_t;

#if !defined(MOSAICO_GAME_ELF) && !defined(RAYLIB_LITE_GAME_NATIVE)
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
#endif

#if !defined(RAYLIB_LITE_GAME_NATIVE)
static void clear_backgrounds(living_worlds_atlases_t *atlases)
{
#if defined(MOSAICO_GAME_ELF)
    /* The product runtime owns the decoded JPEG texture. */
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

static int default_load_background(void *context, living_worlds_atlases_t *atlases,
                                   uint8_t scene)
{
    (void)context;
#if defined(MOSAICO_GAME_ELF)
    raylib_lite_atlas_t photo = raylib_lite_product_jpeg_load(background_path(scene));
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

static void default_release_background(void *context, living_worlds_atlases_t *atlases)
{
    (void)context;
#if defined(MOSAICO_GAME_ELF)
    raylib_lite_product_jpeg_release();
#endif
    clear_backgrounds(atlases);
}

static const living_worlds_session_assets_t s_default_assets = {
    .load_background = default_load_background,
    .release_background = default_release_background,
};

#endif

static int initialize(void *value
#if !defined(MOSAICO_GAME_ELF)
                      , const char *asset_root
#endif
)
{
    module_state_t *module = value;
    living_worlds_session_t *session = &module->session;

#if defined(RAYLIB_LITE_GAME_NATIVE)
    (void)asset_root;
    raylib_lite_result_t result = living_worlds_native_assets_open(
        &module->native_assets);
    if (result != RAYLIB_LITE_OK) return -1;
    if (living_worlds_session_start(session,
            living_worlds_native_assets_callbacks(), module->native_assets) != 0) {
        living_worlds_native_assets_close(module->native_assets);
        module->native_assets = NULL;
        return -1;
    }
#elif !defined(MOSAICO_GAME_ELF)
    raylib_lite_host_assets_set_root(asset_root);
    if (load_atlas(&session->atlases.aurora, "aurora.atlas") ||
        load_atlas(&session->atlases.ocean, "ocean.atlas") ||
        load_atlas(&session->atlases.sunrise, "sunrise.atlas") ||
        load_atlas(&session->atlases.rainforest, "rainforest.atlas")) {
        clear_backgrounds(&session->atlases);
        return -1;
    }
    (void)raylib_lite_2d_cache_texture_light(session->atlases.ocean.texture, 232);
    if (living_worlds_session_start(session, &s_default_assets, session) != 0)
        return -1;
#else
    if (living_worlds_session_start(session, &s_default_assets, session) != 0)
        return -1;
#endif

    InitWindow(480, 480, "Living Worlds");
    SetTargetFPS(30);
    return 0;
}

static void shutdown(void *value)
{
    module_state_t *module = value;
    living_worlds_session_close(&module->session);
#if defined(RAYLIB_LITE_GAME_NATIVE)
    living_worlds_native_assets_close(module->native_assets);
    module->native_assets = NULL;
#endif
}

static void input(void *value, const raylib_lite_game_input_v1_t *event)
{
    module_state_t *module = value;
    living_worlds_session_t *session = &module->session;
    if (!event) return;
    if (event->type == RAYLIB_LITE_GAME_INPUT_POINTER)
        living_worlds_session_pointer(session, (float)event->x, (float)event->y,
                                      event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_ACTION)
        living_worlds_session_action(session, event->code, event->pressed);
    else if (event->type == RAYLIB_LITE_GAME_INPUT_CONTROL) {
        if (event->code == RAYLIB_LITE_GAME_CONTROL_PAUSE) session->paused = true;
        else if (event->code == RAYLIB_LITE_GAME_CONTROL_RESUME) session->paused = false;
        else if (event->code == RAYLIB_LITE_GAME_CONTROL_RESET)
            living_worlds_session_reset(session);
    }
}

static void update(void *value)
{
    module_state_t *module = value;
    (void)living_worlds_session_update(&module->session);
}

static int render(void *value)
{
    module_state_t *module = value;
    living_worlds_session_render(&module->session);
    return 0;
}

static uint32_t state_hash(const void *value)
{
    const module_state_t *module = value;
    return living_world_hash(&module->session.world);
}

static int state_json(const void *value, char *output, size_t capacity)
{
    const module_state_t *module = value;
    const living_world_t *world = &module->session.world;
#if defined(MOSAICO_GAME_ELF)
    return snprintf(output, capacity,
        "{\"scene\":%u,\"yaw\":%d,\"pitch\":%d,\"dragging\":%s,\"tick\":%lu,"
        "\"state_hash\":\"%08lx\"}",
        world->scene, (int)(world->yaw * 100.0f), (int)(world->pitch * 100.0f),
        world->dragging ? "true" : "false",
        (unsigned long)world->tick, (unsigned long)living_world_hash(world));
#else
    return snprintf(output, capacity,
        "{\"scene\":%u,\"yaw\":%.2f,\"pitch\":%.2f,\"dragging\":%s,\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        world->scene, world->yaw, world->pitch, world->dragging ? "true" : "false",
        (unsigned long)world->tick, (unsigned long)living_world_hash(world));
#endif
}

static const raylib_lite_game_module_v1_t s_module = {
    .descriptor = {
        RAYLIB_LITE_GAME_MODULE_ABI,
        "living_worlds", "Living Worlds", 480, 480, 30, 1,
    },
    .state_size = sizeof(module_state_t),
    .initialize = initialize,
    .shutdown = shutdown,
    .input = input,
    .update = update,
    .render = render,
    .state_hash = state_hash,
    .state_json = state_json,
};

#if defined(MOSAICO_GAME_ELF)
RAYLIB_LITE_GAME_MODULE_EXPORT const raylib_lite_game_module_v1_t *
raylib_lite_game_module_v1(const raylib_lite_product_runtime_v1_t *runtime)
{
    if (runtime) raylib_lite_product_runtime = runtime;
    return &s_module;
}
#else
const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void)
{
    return &s_module;
}
#endif
