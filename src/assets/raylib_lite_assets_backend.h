// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_assets.h"

raylib_lite_result_t raylib_lite_asset_mmap_mount(const raylib_lite_asset_store_config_t *config);
void raylib_lite_asset_mmap_unmount(void);
bool raylib_lite_asset_mmap_is_mounted(void);
raylib_lite_result_t raylib_lite_asset_mmap_open(const char *name, raylib_lite_asset_view_t *out);
raylib_lite_result_t raylib_lite_asset_mmap_open_id(raylib_lite_asset_id_t id,
                                     raylib_lite_asset_view_t *out);
