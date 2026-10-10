// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_assets_backend.h"

#include <string.h>
#include "esp_mmap_assets.h"

static mmap_assets_handle_t s_store;

static raylib_lite_result_t from_esp_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return RAYLIB_LITE_OK;
    case ESP_ERR_INVALID_ARG: return RAYLIB_LITE_INVALID_ARGUMENT;
    case ESP_ERR_NO_MEM: return RAYLIB_LITE_NO_MEMORY;
    case ESP_ERR_NOT_FOUND: return RAYLIB_LITE_NOT_FOUND;
    default: return RAYLIB_LITE_PLATFORM_ERROR;
    }
}

raylib_lite_result_t raylib_lite_asset_mmap_mount(
    const raylib_lite_asset_store_config_t *config)
{
    if (!config || !config->partition_label || config->max_files <= 0 || s_store)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    mmap_assets_config_t mmap_config = {
        .partition_label = config->partition_label,
        .max_files = config->max_files,
        .checksum = config->checksum,
        .flags = {
            .mmap_enable = config->mmap_enable,
            .use_fs = false,
            .app_bin_check = true,
        },
    };
    return from_esp_error(mmap_assets_new(&mmap_config, &s_store));
}

void raylib_lite_asset_mmap_unmount(void)
{
    if (s_store) mmap_assets_del(s_store);
    s_store = NULL;
}

bool raylib_lite_asset_mmap_is_mounted(void)
{
    return s_store != NULL;
}

static raylib_lite_result_t open_index(int index, raylib_lite_asset_view_t *out)
{
    if (!s_store || !out || index < 0) return RAYLIB_LITE_INVALID_ARGUMENT;
    const char *name = mmap_assets_get_name(s_store, index);
    const void *data = mmap_assets_get_mem(s_store, index);
    int size = mmap_assets_get_size(s_store, index);
    if (!name || !data || size <= 0) return RAYLIB_LITE_NOT_FOUND;
    *out = (raylib_lite_asset_view_t) {
        .id = raylib_lite_asset_id(name),
        .name = name,
        .data = data,
        .size = (size_t)size,
    };
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_asset_mmap_open(
    const char *name, raylib_lite_asset_view_t *out)
{
    if (!name || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!s_store) return RAYLIB_LITE_NOT_FOUND;
    int count = mmap_assets_get_stored_files(s_store);
    for (int i = 0; i < count; ++i) {
        const char *candidate = mmap_assets_get_name(s_store, i);
        if (candidate && strcmp(candidate, name) == 0) return open_index(i, out);
    }
    return RAYLIB_LITE_NOT_FOUND;
}

raylib_lite_result_t raylib_lite_asset_mmap_open_id(
    raylib_lite_asset_id_t id, raylib_lite_asset_view_t *out)
{
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!s_store) return RAYLIB_LITE_NOT_FOUND;
    int count = mmap_assets_get_stored_files(s_store);
    for (int i = 0; i < count; ++i) {
        const char *name = mmap_assets_get_name(s_store, i);
        if (name && raylib_lite_asset_id(name) == id) return open_index(i, out);
    }
    return RAYLIB_LITE_NOT_FOUND;
}
