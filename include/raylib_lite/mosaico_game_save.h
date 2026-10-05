// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAICO_SAVE_MAX_PAYLOAD 256

typedef esp_err_t (*mosaico_save_storage_read_fn)(
    void *context, const char *namespace_name, const char *key,
    void *data, size_t *size);
typedef esp_err_t (*mosaico_save_storage_write_fn)(
    void *context, const char *namespace_name, const char *key,
    const void *data, size_t size);

typedef struct {
    void *context;
    mosaico_save_storage_read_fn read;
    mosaico_save_storage_write_fn write;
} mosaico_save_storage_t;

typedef esp_err_t (*mosaico_save_migrate_cb_t)(
    uint16_t old_version, const void *old_data, size_t old_size,
    void *new_data, size_t new_size);

typedef struct {
    const mosaico_save_storage_t *storage;
    const char *storage_namespace;
    const char *key;
    uint16_t version;
    size_t payload_size;
    uint32_t debounce_ms;
    mosaico_save_migrate_cb_t migrate;
} mosaico_save_config_t;

typedef struct {
    mosaico_save_config_t config;
    uint8_t pending[MOSAICO_SAVE_MAX_PAYLOAD];
    uint64_t due_ms;
    bool dirty;
    bool initialized;
} mosaico_save_t;

esp_err_t mosaico_save_init(
    mosaico_save_t *save, const mosaico_save_config_t *config);
esp_err_t mosaico_save_load(
    mosaico_save_t *save, void *payload, const void *defaults);
esp_err_t mosaico_save_request(
    mosaico_save_t *save, const void *payload, uint64_t now_ms);
esp_err_t mosaico_save_flush(
    mosaico_save_t *save, uint64_t now_ms, bool force);

/* ESP-IDF NVS backend. Core save logic does not call NVS directly. */
const mosaico_save_storage_t *mosaico_save_nvs_storage(void);

#ifdef __cplusplus
}
#endif
