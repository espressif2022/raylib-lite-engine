// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "mosaico_game_assets.h"

esp_err_t mosaico_asset_mmap_mount(const mosaico_asset_store_config_t *config);
void mosaico_asset_mmap_unmount(void);
bool mosaico_asset_mmap_is_mounted(void);
esp_err_t mosaico_asset_mmap_open(const char *name, mosaico_asset_view_t *out);
esp_err_t mosaico_asset_mmap_open_id(mosaico_asset_id_t id,
                                     mosaico_asset_view_t *out);
