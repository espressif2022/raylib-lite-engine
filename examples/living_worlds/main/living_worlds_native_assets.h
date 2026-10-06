// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_result.h"
#include "living_worlds_session.h"

typedef struct living_worlds_native_assets living_worlds_native_assets_t;

raylib_lite_result_t living_worlds_native_assets_open(
    living_worlds_native_assets_t **out_assets);
const living_worlds_session_assets_t *living_worlds_native_assets_callbacks(void);
void living_worlds_native_assets_close(living_worlds_native_assets_t *assets);
