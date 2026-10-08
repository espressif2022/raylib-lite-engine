// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_save.h"

#include "nvs.h"

static raylib_lite_result_t from_nvs_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return RAYLIB_LITE_OK;
    case ESP_ERR_NVS_NOT_FOUND: return RAYLIB_LITE_NOT_FOUND;
    case ESP_ERR_INVALID_ARG: return RAYLIB_LITE_INVALID_ARGUMENT;
    case ESP_ERR_NO_MEM: return RAYLIB_LITE_NO_MEMORY;
    default: return RAYLIB_LITE_PLATFORM_ERROR;
    }
}

static raylib_lite_result_t nvs_read(void *context, const char *namespace_name,
                                     const char *key, void *data, size_t *size)
{
    (void)context;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READONLY, &handle);
    if (error != ESP_OK) return from_nvs_error(error);
    error = nvs_get_blob(handle, key, data, size);
    nvs_close(handle);
    return from_nvs_error(error);
}

static raylib_lite_result_t nvs_write(void *context, const char *namespace_name,
                                      const char *key, const void *data,
                                      size_t size)
{
    (void)context;
    nvs_handle_t handle = 0;
    esp_err_t error = nvs_open(namespace_name, NVS_READWRITE, &handle);
    if (error == ESP_OK) error = nvs_set_blob(handle, key, data, size);
    if (error == ESP_OK) error = nvs_commit(handle);
    if (handle) nvs_close(handle);
    return from_nvs_error(error);
}

const raylib_lite_save_storage_t *raylib_lite_save_nvs_storage(void)
{
    static const raylib_lite_save_storage_t storage = {
        .read = nvs_read,
        .write = nvs_write,
    };
    return &storage;
}
