// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib_lite_assets.h"
#include "host_asset_runtime.h"

#define HOST_FILE_COUNT 24
typedef struct { char name[64]; uint8_t *data; size_t size; } host_file_t;
static char s_root[512];
static host_file_t s_files[HOST_FILE_COUNT];

void mosaico_host_assets_set_root(const char *root)
{
    snprintf(s_root, sizeof(s_root), "%s", root ? root : "");
}

raylib_lite_asset_id_t raylib_lite_asset_id(const char *name)
{
    uint32_t value = 2166136261U;
    if (!name) return 0;
    while (*name) value = (value ^ (uint8_t)*name++) * 16777619U;
    return value;
}
raylib_lite_result_t raylib_lite_assets_mount(const raylib_lite_asset_store_config_t *config)
{ (void)config; return RAYLIB_LITE_OK; }
void raylib_lite_assets_unmount(void) {}
bool raylib_lite_assets_is_mounted(void) { return true; }

raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    if (!name || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    for (unsigned i = 0; i < HOST_FILE_COUNT; ++i) {
        if (s_files[i].data && strcmp(s_files[i].name, name) == 0) {
            *out = (raylib_lite_asset_view_t){raylib_lite_asset_id(name),
                s_files[i].name, s_files[i].data, s_files[i].size};
            return RAYLIB_LITE_OK;
        }
    }
    char path[640];
    if (snprintf(path, sizeof(path), "%s/%s", s_root, name) >= (int)sizeof(path))
        return RAYLIB_LITE_INVALID_ARGUMENT;
    FILE *file = fopen(path, "rb");
    if (!file) return RAYLIB_LITE_NOT_FOUND;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return RAYLIB_LITE_IO_ERROR; }
    long length = ftell(file); rewind(file);
    if (length <= 0) { fclose(file); return RAYLIB_LITE_IO_ERROR; }
    for (unsigned i = 0; i < HOST_FILE_COUNT; ++i) {
        if (s_files[i].data) continue;
        uint8_t *data = malloc((size_t)length);
        if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
            free(data); fclose(file); return RAYLIB_LITE_IO_ERROR;
        }
        fclose(file);
        snprintf(s_files[i].name, sizeof(s_files[i].name), "%s", name);
        s_files[i].data = data; s_files[i].size = (size_t)length;
        *out = (raylib_lite_asset_view_t){raylib_lite_asset_id(name),
            s_files[i].name, data, (size_t)length};
        return RAYLIB_LITE_OK;
    }
    fclose(file);
    return RAYLIB_LITE_NO_MEMORY;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (!view) return;
    view->data = NULL;
    view->size = 0;
}

raylib_lite_result_t raylib_lite_asset_open_id(raylib_lite_asset_id_t id,
                                     raylib_lite_asset_view_t *out)
{
    static const char *names[] = {
        "tower.atlas", "terrain.atlas", "level01.map", NULL};
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    for (unsigned i = 0; i < HOST_FILE_COUNT; ++i) {
        if (s_files[i].data && raylib_lite_asset_id(s_files[i].name) == id)
            return raylib_lite_asset_open(s_files[i].name, out);
    }
    for (const char **name = names; *name; ++name) {
        if (raylib_lite_asset_id(*name) == id)
            return raylib_lite_asset_open(*name, out);
    }
    return RAYLIB_LITE_NOT_FOUND;
}
