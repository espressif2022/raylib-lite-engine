// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAYLIB_LITE_SAVE_MAX_PAYLOAD 256

typedef raylib_lite_result_t (*raylib_lite_save_storage_read_fn)(
    void *context, const char *namespace_name, const char *key,
    void *data, size_t *size);
typedef raylib_lite_result_t (*raylib_lite_save_storage_write_fn)(
    void *context, const char *namespace_name, const char *key,
    const void *data, size_t size);

typedef struct {
    void *context;
    raylib_lite_save_storage_read_fn read;
    raylib_lite_save_storage_write_fn write;
} raylib_lite_save_storage_t;

typedef raylib_lite_result_t (*raylib_lite_save_migrate_cb_t)(
    uint16_t old_version, const void *old_data, size_t old_size,
    void *new_data, size_t new_size);

typedef struct {
    const raylib_lite_save_storage_t *storage;
    const char *storage_namespace;
    const char *key;
    uint16_t version;
    size_t payload_size;
    uint32_t debounce_ms;
    raylib_lite_save_migrate_cb_t migrate;
} raylib_lite_save_config_t;

typedef struct {
    raylib_lite_save_config_t config;
    uint8_t pending[RAYLIB_LITE_SAVE_MAX_PAYLOAD];
    uint64_t due_ms;
    bool dirty;
    bool initialized;
} raylib_lite_save_t;

raylib_lite_result_t raylib_lite_save_init(
    raylib_lite_save_t *save, const raylib_lite_save_config_t *config);
raylib_lite_result_t raylib_lite_save_load(
    raylib_lite_save_t *save, void *payload, const void *defaults);
raylib_lite_result_t raylib_lite_save_request(
    raylib_lite_save_t *save, const void *payload, uint64_t now_ms);
raylib_lite_result_t raylib_lite_save_flush(
    raylib_lite_save_t *save, uint64_t now_ms, bool force);

/* ESP-IDF NVS backend. Core save logic does not call NVS directly. */
const raylib_lite_save_storage_t *raylib_lite_save_nvs_storage(void);

#ifdef __cplusplus
}
#endif
