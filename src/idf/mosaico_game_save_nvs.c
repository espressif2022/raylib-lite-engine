// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_save.h"

#include "nvs.h"

static esp_err_t nvs_read(void *context, const char *namespace_name,
                          const char *key, void *data, size_t *size)
{
    (void)context;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NOT_FOUND;
    if (error != ESP_OK) return error;
    error = nvs_get_blob(handle, key, data, size);
    nvs_close(handle);
    return error == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : error;
}

static esp_err_t nvs_write(void *context, const char *namespace_name,
                           const char *key, const void *data, size_t size)
{
    (void)context;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (error == ESP_OK) error = nvs_set_blob(handle, key, data, size);
    if (error == ESP_OK) error = nvs_commit(handle);
    if (handle) nvs_close(handle);
    return error;
}

const mosaico_save_storage_t *mosaico_save_nvs_storage(void)
{
    static const mosaico_save_storage_t storage = {
        .read = nvs_read,
        .write = nvs_write,
    };
    return &storage;
}
