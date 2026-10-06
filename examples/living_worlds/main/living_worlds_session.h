// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "living_worlds_view.h"
#if defined(MOSAICO_GAME_ELF) || defined(RAYLIB_LITE_GAME_NATIVE)
#include "living_worlds_scene_audio.h"
#endif

typedef struct {
    int (*load_background)(void *context, living_worlds_atlases_t *atlases,
                           uint8_t scene);
    void (*release_background)(void *context, living_worlds_atlases_t *atlases);
} living_worlds_session_assets_t;

typedef struct {
    living_world_t world;
    living_worlds_atlases_t atlases;
#if defined(MOSAICO_GAME_ELF) || defined(RAYLIB_LITE_GAME_NATIVE)
    living_worlds_scene_audio_t audio;
    bool audio_started;
#endif
    const living_worlds_session_assets_t *assets;
    void *asset_context;
    uint8_t loaded_volumes;
    bool paused;
} living_worlds_session_t;

int living_worlds_session_start(living_worlds_session_t *session,
                                const living_worlds_session_assets_t *assets,
                                void *asset_context);
void living_worlds_session_pointer(living_worlds_session_t *session,
                                   float x, float y, bool pressed);
void living_worlds_session_action(living_worlds_session_t *session,
                                  int code, bool pressed);
void living_worlds_session_reset(living_worlds_session_t *session);
bool living_worlds_session_update(living_worlds_session_t *session);
void living_worlds_session_render(living_worlds_session_t *session);
void living_worlds_session_close(living_worlds_session_t *session);
