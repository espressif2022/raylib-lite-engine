// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t raylib_lite_asset_id_t;
typedef enum {
    RAYLIB_LITE_ASSET_BINARY=0, RAYLIB_LITE_ASSET_PNG, RAYLIB_LITE_ASSET_FONT,
    RAYLIB_LITE_ASSET_AUDIO, RAYLIB_LITE_ASSET_ATLAS, RAYLIB_LITE_ASSET_TILEMAP
} raylib_lite_asset_type_t;
typedef struct { raylib_lite_asset_id_t id; raylib_lite_asset_type_t type; const uint8_t *data; size_t size; } raylib_lite_asset_t;

typedef struct {
    raylib_lite_asset_id_t id;
    const char *name;
    const uint8_t *data;
    size_t size;
} raylib_lite_asset_view_t;

typedef struct {
    const char *partition_label;
    int max_files;
    uint16_t checksum;
    bool mmap_enable;
    /* In-memory MMAP blob from an ELF bundle. When set, mount parses this
     * image instead of a flash partition. */
    const uint8_t *image;
    size_t image_size;
} raylib_lite_asset_store_config_t;

const raylib_lite_asset_t *raylib_lite_asset_find(const raylib_lite_asset_t *assets,size_t count,raylib_lite_asset_id_t id);
raylib_lite_asset_id_t raylib_lite_asset_id(const char *name);
raylib_lite_result_t raylib_lite_assets_mount(const raylib_lite_asset_store_config_t *config);
raylib_lite_result_t raylib_lite_asset_register_memory(const char *name,
                                              const void *data, size_t size);
void raylib_lite_assets_unmount(void);
bool raylib_lite_assets_is_mounted(void);
raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out);
raylib_lite_result_t raylib_lite_asset_open_id(raylib_lite_asset_id_t id,
                                     raylib_lite_asset_view_t *out);

/* Bounded read over an MMAP image; offset 0 is the image header. The callback
 * must either fill the requested range or return an error. */
typedef raylib_lite_result_t (*raylib_lite_asset_backing_read_fn)(void *ctx,
        uint32_t offset, void *dst, size_t len);

typedef struct {
    uint32_t size;
    void *ctx;
    raylib_lite_asset_backing_read_fn read;
    /* Optional resident alias of the full image. Non-NULL selects zero-copy
     * views; the caller owns the alias and keeps it valid until unmount. */
    const uint8_t *image;
} raylib_lite_asset_backing_t;

raylib_lite_result_t raylib_lite_assets_mount_backing(
        const raylib_lite_asset_backing_t *backing, int max_files,
        uint16_t checksum);
void raylib_lite_asset_release(raylib_lite_asset_view_t *view);

typedef struct raylib_lite_asset_stream raylib_lite_asset_stream_t;
raylib_lite_result_t raylib_lite_asset_stream_open(const char *name, uint32_t skip,
                                         raylib_lite_asset_stream_t **out);
raylib_lite_result_t raylib_lite_asset_stream_read(raylib_lite_asset_stream_t *stream,
                                         void *dst, size_t len,
                                         size_t *read_out);
void raylib_lite_asset_stream_close(raylib_lite_asset_stream_t *stream);

size_t raylib_lite_assets_toc_bytes(void);
size_t raylib_lite_assets_materialized_bytes(void);
size_t raylib_lite_assets_materialized_peak(void);

#ifdef __cplusplus
}
#endif
