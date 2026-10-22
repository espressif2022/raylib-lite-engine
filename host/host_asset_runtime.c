// SPDX-License-Identifier: Apache-2.0
/* Host file backend for the same asset core used by native applications. */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "raylib_lite_assets_backend.h"
#include "host_asset_runtime.h"

#define HOST_FILE_COUNT 256
#define HOST_NAME_LENGTH 256
static char s_root[1024];
typedef struct {
    char name[HOST_NAME_LENGTH];
    raylib_lite_asset_id_t id;
    uint8_t *data;
    size_t size;
} host_file_t;
static host_file_t s_files[HOST_FILE_COUNT];
static size_t s_count;
static bool s_mounted;
static raylib_lite_result_t s_error = RAYLIB_LITE_NOT_READY;

void raylib_lite_asset_mmap_unmount(void)
{
    for (size_t i = 0; i < s_count; ++i) free(s_files[i].data);
    memset(s_files, 0, sizeof(s_files));
    s_count = 0;
    s_mounted = false;
}

/* Index names before any load so ID lookup never depends on loading order.
 * Generated asset directories are flat; subdirectories are not indexed. */
static raylib_lite_result_t index_root(void)
{
    DIR *directory = opendir(s_root);
    if (!directory) return RAYLIB_LITE_NOT_FOUND;
    raylib_lite_result_t result = RAYLIB_LITE_OK;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char path[1400];
        struct stat info;
        int length = snprintf(path, sizeof(path), "%s/%s", s_root, entry->d_name);
        if (length < 0 || length >= (int)sizeof(path)) {
            result = RAYLIB_LITE_INVALID_SIZE; break;
        }
        if (stat(path, &info) != 0) { result = RAYLIB_LITE_IO_ERROR; break; }
        if (!S_ISREG(info.st_mode)) continue;
        if (s_count == HOST_FILE_COUNT || strlen(entry->d_name) >= HOST_NAME_LENGTH) {
            result = RAYLIB_LITE_INVALID_SIZE; break;
        }
        raylib_lite_asset_id_t id = raylib_lite_asset_id(entry->d_name);
        for (size_t i = 0; i < s_count; ++i)
            if (s_files[i].id == id) { result = RAYLIB_LITE_INVALID_ARGUMENT; break; }
        if (result != RAYLIB_LITE_OK) break;
        host_file_t *file = &s_files[s_count++];
        strcpy(file->name, entry->d_name);
        file->id = id;
    }
    closedir(directory);
    return result;
}

static void mount_root(const char *root)
{
    raylib_lite_asset_mmap_unmount();
    int length = snprintf(s_root, sizeof(s_root), "%s", root ? root : "");
    s_error = length < 0 || length >= (int)sizeof(s_root)
        ? RAYLIB_LITE_INVALID_SIZE : index_root();
    s_mounted = s_error == RAYLIB_LITE_OK;
    if (!s_mounted) raylib_lite_asset_mmap_unmount();
}

void raylib_lite_host_assets_set_root(const char *root)
{
    /* Explicit root switching resets the complete runtime. A regular asset
     * mount below preserves separately registered memory resources. */
    raylib_lite_assets_unmount();
    mount_root(root);
}

raylib_lite_result_t raylib_lite_asset_mmap_mount(const raylib_lite_asset_store_config_t *config)
{
    if (!config || !config->partition_label) return RAYLIB_LITE_INVALID_ARGUMENT;
    mount_root(config->partition_label);
    return s_error;
}
bool raylib_lite_asset_mmap_is_mounted(void) { return s_mounted; }

static raylib_lite_result_t open_file(host_file_t *entry, raylib_lite_asset_view_t *out)
{
    if (!entry->data) {
        char path[1400];
        snprintf(path, sizeof(path), "%s/%s", s_root, entry->name);
        FILE *file = fopen(path, "rb");
        if (!file) return RAYLIB_LITE_NOT_FOUND;
        if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return RAYLIB_LITE_IO_ERROR; }
        long length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            fclose(file); return RAYLIB_LITE_IO_ERROR;
        }
        uint8_t *data = malloc(length ? (size_t)length : 1);
        if (!data) { fclose(file); return RAYLIB_LITE_NO_MEMORY; }
        if (fread(data, 1, (size_t)length, file) != (size_t)length) {
            free(data); fclose(file); return RAYLIB_LITE_IO_ERROR;
        }
        fclose(file);
        entry->data = data;
        entry->size = (size_t)length;
    }
    *out = (raylib_lite_asset_view_t){entry->id, entry->name, entry->data, entry->size};
    return RAYLIB_LITE_OK;
}
raylib_lite_result_t raylib_lite_asset_mmap_open(const char *name, raylib_lite_asset_view_t *out)
{
    if (!name || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!s_mounted) return s_error == RAYLIB_LITE_OK ? RAYLIB_LITE_NOT_READY : s_error;
    for (size_t i = 0; i < s_count; ++i)
        if (strcmp(s_files[i].name, name) == 0) return open_file(&s_files[i], out);
    return RAYLIB_LITE_NOT_FOUND;
}
raylib_lite_result_t raylib_lite_asset_mmap_open_id(raylib_lite_asset_id_t id,
                                                 raylib_lite_asset_view_t *out)
{
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!s_mounted) return s_error == RAYLIB_LITE_OK ? RAYLIB_LITE_NOT_READY : s_error;
    for (size_t i = 0; i < s_count; ++i)
        if (s_files[i].id == id) return open_file(&s_files[i], out);
    return RAYLIB_LITE_NOT_FOUND;
}
