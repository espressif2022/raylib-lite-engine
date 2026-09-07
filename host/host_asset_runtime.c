// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mosaico_game_assets.h"
#include "host_asset_runtime.h"

#define HOST_FILE_COUNT 24
typedef struct { char name[64]; uint8_t *data; size_t size; } host_file_t;
static char s_root[512];
static host_file_t s_files[HOST_FILE_COUNT];

void mosaico_host_assets_set_root(const char *root)
{
    snprintf(s_root, sizeof(s_root), "%s", root ? root : "");
}

mosaico_asset_id_t mosaico_game_asset_id(const char *name)
{
    uint32_t value = 2166136261U;
    if (!name) return 0;
    while (*name) value = (value ^ (uint8_t)*name++) * 16777619U;
    return value;
}
esp_err_t mosaico_game_assets_mount(const mosaico_asset_store_config_t *config)
{ (void)config; return ESP_OK; }
void mosaico_game_assets_unmount(void) {}
bool mosaico_game_assets_is_mounted(void) { return true; }

esp_err_t mosaico_game_asset_open(const char *name, mosaico_asset_view_t *out)
{
    if (!name || !out) return ESP_ERR_INVALID_ARG;
    for (unsigned i = 0; i < HOST_FILE_COUNT; ++i) {
        if (s_files[i].data && strcmp(s_files[i].name, name) == 0) {
            *out = (mosaico_asset_view_t){mosaico_game_asset_id(name),
                s_files[i].name, s_files[i].data, s_files[i].size};
            return ESP_OK;
        }
    }
    char path[640];
    if (snprintf(path, sizeof(path), "%s/%s", s_root, name) >= (int)sizeof(path))
        return ESP_ERR_INVALID_ARG;
    FILE *file = fopen(path, "rb");
    if (!file) return ESP_ERR_NOT_FOUND;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return ESP_FAIL; }
    long length = ftell(file); rewind(file);
    if (length <= 0) { fclose(file); return ESP_FAIL; }
    for (unsigned i = 0; i < HOST_FILE_COUNT; ++i) {
        if (s_files[i].data) continue;
        uint8_t *data = malloc((size_t)length);
        if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
            free(data); fclose(file); return ESP_FAIL;
        }
        fclose(file);
        snprintf(s_files[i].name, sizeof(s_files[i].name), "%s", name);
        s_files[i].data = data; s_files[i].size = (size_t)length;
        *out = (mosaico_asset_view_t){mosaico_game_asset_id(name),
            s_files[i].name, data, (size_t)length};
        return ESP_OK;
    }
    fclose(file);
    return ESP_ERR_NO_MEM;
}

esp_err_t mosaico_game_asset_open_id(mosaico_asset_id_t id,
                                     mosaico_asset_view_t *out)
{
    return id == mosaico_game_asset_id("tower.atlas")
        ? mosaico_game_asset_open("tower.atlas", out) : ESP_ERR_NOT_FOUND;
}
