// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../../raylib_lite_platform/include/raylib_lite_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mosaico_asset_id_t;
typedef enum {
    MOSAICO_ASSET_BINARY=0, MOSAICO_ASSET_PNG, MOSAICO_ASSET_FONT,
    MOSAICO_ASSET_AUDIO, MOSAICO_ASSET_ATLAS, MOSAICO_ASSET_TILEMAP
} mosaico_asset_type_t;
typedef struct { mosaico_asset_id_t id; mosaico_asset_type_t type; const uint8_t *data; size_t size; } mosaico_asset_t;

typedef struct {
    mosaico_asset_id_t id;
    const char *name;
    const uint8_t *data;
    size_t size;
} mosaico_asset_view_t;

typedef struct {
    const char *partition_label;
    int max_files;
    uint16_t checksum;
    bool mmap_enable;
    /* In-memory MMAP blob from an ELF bundle. When set, mount parses this
     * image instead of a flash partition. */
    const uint8_t *image;
    size_t image_size;
} mosaico_asset_store_config_t;

const mosaico_asset_t *mosaico_game_asset_find(const mosaico_asset_t *assets,size_t count,mosaico_asset_id_t id);
mosaico_asset_id_t mosaico_game_asset_id(const char *name);
esp_err_t mosaico_game_assets_mount(const mosaico_asset_store_config_t *config);
esp_err_t mosaico_game_asset_register_memory(const char *name,
                                              const void *data, size_t size);
void mosaico_game_assets_unmount(void);
bool mosaico_game_assets_is_mounted(void);
esp_err_t mosaico_game_asset_open(const char *name, mosaico_asset_view_t *out);
esp_err_t mosaico_game_asset_open_id(mosaico_asset_id_t id,
                                     mosaico_asset_view_t *out);

/* Bounded read over an MMAP image; offset 0 is the image header. The callback
 * must either fill the requested range or return an error. */
typedef esp_err_t (*mosaico_asset_backing_read_fn)(void *ctx,
        uint32_t offset, void *dst, size_t len);

typedef struct {
    uint32_t size;
    void *ctx;
    mosaico_asset_backing_read_fn read;
    /* Optional resident alias of the full image. Non-NULL selects zero-copy
     * views; the caller owns the alias and keeps it valid until unmount. */
    const uint8_t *image;
} mosaico_asset_backing_t;

esp_err_t mosaico_game_assets_mount_backing(
        const mosaico_asset_backing_t *backing, int max_files,
        uint16_t checksum);
void mosaico_game_asset_release(mosaico_asset_view_t *view);

typedef struct mosaico_asset_stream mosaico_asset_stream_t;
esp_err_t mosaico_game_asset_stream_open(const char *name, uint32_t skip,
                                         mosaico_asset_stream_t **out);
esp_err_t mosaico_game_asset_stream_read(mosaico_asset_stream_t *stream,
                                         void *dst, size_t len,
                                         size_t *read_out);
void mosaico_game_asset_stream_close(mosaico_asset_stream_t *stream);

size_t mosaico_game_assets_toc_bytes(void);
size_t mosaico_game_assets_materialized_bytes(void);
size_t mosaico_game_assets_materialized_peak(void);

#ifdef __cplusplus
}
#endif
