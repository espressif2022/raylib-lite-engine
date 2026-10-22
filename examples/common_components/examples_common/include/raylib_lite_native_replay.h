// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "raylib_lite_game_module.h"

typedef struct {
    uint32_t duration_ticks;
    uint32_t event_count;
    const char *source_sha256;
    const char *events_sha256;
} raylib_lite_native_replay_info_t;

/* Implemented by tools/native_replay.py's generated translation unit.
 * Call once per logical tick starting at zero, before the module update.
 * True means this is the last tick: update once, then close normally. */
extern const raylib_lite_native_replay_info_t raylib_lite_native_replay_info;
bool raylib_lite_native_replay_tick(uint32_t tick,
    const raylib_lite_game_module_v1_t *module, void *state);
