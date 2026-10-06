// SPDX-License-Identifier: Apache-2.0
#include "esp_mosaico_iris.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_iris_system_inventory.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "psa/crypto.h"
#include "sdkconfig.h"

#if !CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY
#error "ESP-Mosaico native applications must use retained Recovery for OTA"
#endif

#if CONFIG_ESP_IRIS_OTA
#error "The ESP-Iris OTA writer belongs in retained Recovery, not normal Games"
#endif

#define POINTER_SERVICE_ID 0x1001U
#define POINTER_METHOD_ID 1U
#define POINTER_MESSAGE_SIZE 12U
#define COPY_ATTEMPTS 3

#define OTA_SERVICE_ID 0x1200U
#define OTA_STATE_METHOD_ID 1U
#define OTA_ACCEPT_METHOD_ID 2U
#define RECOVERY_SERVICE_ID 0x7FFFU
#define ENTER_RECOVERY_METHOD 2U
#define RECOVERY_OTA_NAMESPACE "iris_ota_demo"

#define SYSTEM_METADATA_PARTITION "sysmeta"
#define SYSTEM_UPDATE_NAMESPACE "update"
#define SYSTEM_UPDATE_RESULT_KEY "last_result"
#define SYSTEM_METADATA_MAGIC 0x49535953U
#define SYSTEM_METADATA_VERSION 2U
#define SYSTEM_LAYOUT_VERSION 4U
#define SYSTEM_HASH_CHUNK_BYTES 1024U

static const char *TAG = "esp_mosaico_iris";

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    int32_t result;
    uint8_t reserved[36];
} system_metadata_record_t;

typedef struct {
    raylib_lite_video_backend_t video;
    raylib_lite_input_queue_t *input;
    uint16_t width;
    uint16_t height;
    uint16_t *capture;
    bool started;
} esp_mosaico_iris_state_t;

static esp_mosaico_iris_state_t s_iris;

static esp_err_t to_esp(raylib_lite_result_t result)
{
    switch (result) {
    case RAYLIB_LITE_OK:
        return ESP_OK;
    case RAYLIB_LITE_INVALID_ARGUMENT:
        return ESP_ERR_INVALID_ARG;
    case RAYLIB_LITE_INVALID_STATE:
    case RAYLIB_LITE_NOT_READY:
        return ESP_ERR_INVALID_STATE;
    case RAYLIB_LITE_NO_MEMORY:
        return ESP_ERR_NO_MEM;
    case RAYLIB_LITE_NOT_SUPPORTED:
        return ESP_ERR_NOT_SUPPORTED;
    case RAYLIB_LITE_BUSY:
    case RAYLIB_LITE_TIMEOUT:
        return ESP_ERR_TIMEOUT;
    default:
        return ESP_FAIL;
    }
}

static bool is_ota_partition(const esp_partition_t *partition)
{
    return partition != NULL &&
           partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
           partition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX;
}

static esp_err_t recovery_write(uint32_t last_good, uint32_t target)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(
        nvs_open_from_partition(CONFIG_ESP_IRIS_NVS_PARTITION_NAME,
                                RECOVERY_OTA_NAMESPACE, NVS_READWRITE,
                                &handle),
        TAG, "open recovery metadata");

    esp_err_t err = nvs_set_u32(handle, "last_good", last_good);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, "target", target);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static uint32_t recovery_read_u32(const char *key)
{
    nvs_handle_t handle;
    uint32_t value = 0;
    if (nvs_open_from_partition(CONFIG_ESP_IRIS_NVS_PARTITION_NAME,
                                RECOVERY_OTA_NAMESPACE, NVS_READONLY,
                                &handle) == ESP_OK) {
        (void)nvs_get_u32(handle, key, &value);
        nvs_close(handle);
    }
    return value;
}

static esp_err_t hash_flash_region(
    uint32_t address, size_t size,
    uint8_t output[ESP_IRIS_SYSTEM_SHA256_BYTES])
{
    uint8_t *buffer = heap_caps_malloc(
        SYSTEM_HASH_CHUNK_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate Flash hash buffer");

    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    if (psa_crypto_init() != PSA_SUCCESS ||
            psa_hash_setup(&operation, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        heap_caps_free(buffer);
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    for (size_t offset = 0; offset < size;) {
        size_t chunk = size - offset;
        if (chunk > SYSTEM_HASH_CHUNK_BYTES) {
            chunk = SYSTEM_HASH_CHUNK_BYTES;
        }
        err = esp_flash_read(NULL, buffer, address + offset, chunk);
        if (err != ESP_OK ||
                psa_hash_update(&operation, buffer, chunk) != PSA_SUCCESS) {
            err = err == ESP_OK ? ESP_FAIL : err;
            break;
        }
        offset += chunk;
    }

    size_t output_size = 0;
    if (err == ESP_OK &&
            (psa_hash_finish(&operation, output,
                             ESP_IRIS_SYSTEM_SHA256_BYTES, &output_size) !=
                 PSA_SUCCESS ||
             output_size != ESP_IRIS_SYSTEM_SHA256_BYTES)) {
        err = ESP_FAIL;
    }
    if (err != ESP_OK) {
        (void)psa_hash_abort(&operation);
    }
    heap_caps_free(buffer);
    return err;
}

static void system_inventory_load_last_result(
    esp_iris_system_inventory_t *inventory)
{
    nvs_handle_t handle;
    if (nvs_open_from_partition(SYSTEM_METADATA_PARTITION,
                                SYSTEM_UPDATE_NAMESPACE, NVS_READONLY,
                                &handle) != ESP_OK) {
        return;
    }

    system_metadata_record_t record;
    size_t size = sizeof(record);
    const esp_err_t err = nvs_get_blob(
        handle, SYSTEM_UPDATE_RESULT_KEY, &record, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(record) ||
            record.magic != SYSTEM_METADATA_MAGIC ||
            record.version != SYSTEM_METADATA_VERSION) {
        return;
    }

    memcpy(inventory->last_operation_id, record.operation_id,
           sizeof(inventory->last_operation_id));
    inventory->last_result = record.result;
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_LAST_OPERATION;
}

static esp_err_t system_inventory_get(
    esp_iris_system_inventory_t *inventory, void *user_ctx)
{
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(inventory != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "inventory is null");
    memset(inventory, 0, sizeof(*inventory));
    inventory->layout_version = SYSTEM_LAYOUT_VERSION;

    const uint32_t bootloader_offset = CONFIG_BOOTLOADER_OFFSET_IN_FLASH;
    const uint32_t partition_offset = CONFIG_PARTITION_TABLE_OFFSET;
    ESP_RETURN_ON_FALSE(partition_offset > bootloader_offset,
                        ESP_ERR_INVALID_STATE, TAG,
                        "invalid protected Flash ranges");

    ESP_RETURN_ON_ERROR(
        hash_flash_region(bootloader_offset,
                          partition_offset - bootloader_offset,
                          inventory->bootloader_sha256),
        TAG, "hash bootloader range");
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_BOOTLOADER_SHA256;

    ESP_RETURN_ON_ERROR(
        hash_flash_region(partition_offset, 0x1000,
                          inventory->partition_table_sha256),
        TAG, "hash partition table");
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_PARTITION_TABLE_SHA256;

    system_inventory_load_last_result(inventory);
    return ESP_OK;
}

static esp_err_t system_inventory_register(void)
{
    const esp_iris_system_inventory_provider_t provider = {
        .get_inventory = system_inventory_get,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(
        esp_iris_system_inventory_register(&provider),
        TAG, "register system inventory");
    return ESP_OK;
}

static esp_err_t copy_latest_frame(esp_mosaico_iris_state_t *state)
{
    const size_t pixels = (size_t)state->width * state->height;
    for (int attempt = 0; attempt < COPY_ATTEMPTS; ++attempt) {
        raylib_lite_result_t result =
            state->video.copy_latest(state->video.context, state->capture, pixels);
        if (result == RAYLIB_LITE_OK) {
            return ESP_OK;
        }
        if (result != RAYLIB_LITE_BUSY && result != RAYLIB_LITE_TIMEOUT) {
            return to_esp(result);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_ERR_TIMEOUT;
}

static esp_err_t screen_begin(const esp_iris_media_desc_t *requested,
                              esp_iris_media_desc_t *actual,
                              uint32_t *total_size,
                              void *user_ctx)
{
    (void)requested;
    esp_mosaico_iris_state_t *state = user_ctx;
    if (!state || !actual || !total_size || state->capture) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t pixels = (size_t)state->width * state->height;
    const size_t bytes = pixels * sizeof(uint16_t);
    if (bytes > UINT32_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }

    state->capture = heap_caps_malloc(
        bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!state->capture) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = copy_latest_frame(state);
    if (err != ESP_OK) {
        heap_caps_free(state->capture);
        state->capture = NULL;
        return err;
    }

    *actual = (esp_iris_media_desc_t) {
        .x = 0,
        .y = 0,
        .width = state->width,
        .height = state->height,
        .stride = (uint32_t)state->width * sizeof(uint16_t),
        .format = ESP_IRIS_PIXEL_FORMAT_RGB565,
        .quality = 0,
    };
    *total_size = (uint32_t)bytes;
    return ESP_OK;
}

static esp_err_t screen_read(uint32_t offset,
                             uint8_t *out,
                             size_t capacity,
                             size_t *out_size,
                             void *user_ctx)
{
    esp_mosaico_iris_state_t *state = user_ctx;
    if (!state || !state->capture || !out || !out_size || capacity == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t total =
        (size_t)state->width * state->height * sizeof(uint16_t);
    if (offset >= total) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t size = total - offset;
    if (size > capacity) {
        size = capacity;
    }
    memcpy(out, (const uint8_t *)state->capture + offset, size);
    *out_size = size;
    return ESP_OK;
}

static void screen_end(void *user_ctx)
{
    esp_mosaico_iris_state_t *state = user_ctx;
    if (!state) {
        return;
    }
    heap_caps_free(state->capture);
    state->capture = NULL;
}

static int16_t read_le_i16(const uint8_t *value)
{
    return (int16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8));
}

static void write_le_i16(uint8_t *value, int16_t data)
{
    value[0] = (uint8_t)data;
    value[1] = (uint8_t)((uint16_t)data >> 8);
}

static esp_err_t pointer_rpc(const esp_iris_rpc_request_t *request,
                             uint8_t *response,
                             size_t response_capacity,
                             size_t *response_size,
                             void *user_ctx)
{
    esp_mosaico_iris_state_t *state = user_ctx;
    if (!state || !request || request->payload_size != POINTER_MESSAGE_SIZE ||
            !response || response_capacity < POINTER_MESSAGE_SIZE ||
            !response_size || request->payload[0] > 2) {
        return ESP_ERR_INVALID_SIZE;
    }

    int32_t x = read_le_i16(request->payload + 2);
    int32_t y = read_le_i16(request->payload + 4);
    if (x < 0) {
        x = 0;
    } else if (x >= state->width) {
        x = state->width - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y >= state->height) {
        y = state->height - 1;
    }

    raylib_lite_input_event_t event = {
        .type = RAYLIB_LITE_INPUT_POINTER,
        .x = x,
        .y = y,
        .pressed = request->payload[0] != 2,
        .timestamp_us = (uint64_t)esp_timer_get_time(),
    };
    raylib_lite_result_t result = raylib_lite_input_push(state->input, &event);
    if (result != RAYLIB_LITE_OK) {
        return to_esp(result);
    }

    memcpy(response, request->payload, POINTER_MESSAGE_SIZE);
    write_le_i16(response + 2, (int16_t)x);
    write_le_i16(response + 4, (int16_t)y);
    *response_size = POINTER_MESSAGE_SIZE;
    return ESP_OK;
}

esp_err_t esp_iris_platform_mark_healthy(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!is_ota_partition(running)) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(
        esp_ota_mark_app_valid_cancel_rollback(),
        TAG, "accept pending image");
    return recovery_write(running->address, 0);
}

static esp_err_t state_rpc(const esp_iris_rpc_request_t *request,
                           uint8_t *response,
                           size_t response_capacity,
                           size_t *response_size,
                           void *user_ctx)
{
    (void)user_ctx;
    if (!request || request->payload_size != 0 || !response ||
            !response_size) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    const esp_app_desc_t *app = esp_app_get_description();
    if (!running || !next || !app) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t image_state;
    const esp_err_t state_err =
        esp_ota_get_state_partition(running, &image_state);
    esp_iris_status_t iris_status = {0};
    (void)esp_iris_get_status(&iris_status);

    const int written = snprintf(
        (char *)response, response_capacity,
        "{\"project\":\"%s\",\"version\":\"%s\",\"mode\":\"normal\","
        "\"ota_execution\":\"recovery\",\"ota_writer\":false,"
        "\"running\":\"%s\",\"next\":\"%s\",\"image_state\":%d,"
        "\"last_good\":%" PRIu32 ",\"target\":%" PRIu32
        ",\"crash_count\":%" PRIu32 ",\"crash_recovery_pending\":%u}",
        app->project_name, app->version, running->label, next->label,
        state_err == ESP_OK ? (int)image_state : -1,
        recovery_read_u32("last_good"), recovery_read_u32("target"),
        iris_status.crash_count,
        iris_status.crash_recovery_pending ? 1U : 0U);
    if (written < 0 || (size_t)written >= response_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    *response_size = (size_t)written;
    return ESP_OK;
}

static esp_err_t accept_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *response,
                            size_t response_capacity,
                            size_t *response_size,
                            void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    if (!request || request->payload_size != 0 || !response_size) {
        return ESP_ERR_INVALID_ARG;
    }
    *response_size = 0;
    return esp_iris_mark_healthy();
}

static void enter_recovery_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t enter_recovery(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (!running || !factory || running->address == factory->address) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(
        esp_iris_mark_planned_restart(), TAG, "record recovery restart");
    ESP_RETURN_ON_ERROR(
        esp_ota_set_boot_partition(factory), TAG, "select factory recovery");
    if (xTaskCreate(enter_recovery_task, "enter_recovery", 2048, NULL, 5,
                    NULL) != pdPASS) {
        (void)esp_ota_set_boot_partition(running);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static esp_err_t enter_recovery_rpc(const esp_iris_rpc_request_t *request,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t *response_size,
                                    void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    if (!request || request->payload_size != 0 || !response_size) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        enter_recovery(), TAG, "schedule factory recovery");
    *response_size = 0;
    return ESP_OK;
}

static void unregister_prestart(void)
{
    (void)esp_iris_rpc_unregister(POINTER_SERVICE_ID, POINTER_METHOD_ID);
    (void)esp_iris_screen_unregister(&s_iris);
    (void)esp_iris_rpc_unregister(
        RECOVERY_SERVICE_ID, ENTER_RECOVERY_METHOD);
    (void)esp_iris_rpc_unregister(
        OTA_SERVICE_ID, OTA_ACCEPT_METHOD_ID);
    (void)esp_iris_rpc_unregister(
        OTA_SERVICE_ID, OTA_STATE_METHOD_ID);
    (void)esp_iris_system_inventory_unregister(NULL);
}

esp_err_t esp_mosaico_iris_boot_probe(void)
{
    return esp_iris_boot_probe();
}

esp_err_t esp_mosaico_iris_start(raylib_lite_video_backend_t video,
                                 raylib_lite_input_queue_t *input,
                                 uint16_t width,
                                 uint16_t height)
{
    if (s_iris.started || !input || !width || !height || !video.copy_latest) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_iris, 0, sizeof(s_iris));
    s_iris.video = video;
    s_iris.input = input;
    s_iris.width = width;
    s_iris.height = height;

    esp_err_t err = nvs_flash_init_partition(SYSTEM_METADATA_PARTITION);
    if (err != ESP_OK) {
        return err;
    }
    err = system_inventory_register();
    if (err != ESP_OK) {
        return err;
    }
    err = esp_iris_rpc_register(
        OTA_SERVICE_ID, OTA_STATE_METHOD_ID, state_rpc, NULL);
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }
    err = esp_iris_rpc_register(
        OTA_SERVICE_ID, OTA_ACCEPT_METHOD_ID, accept_rpc, NULL);
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }
    err = esp_iris_rpc_register(
        RECOVERY_SERVICE_ID, ENTER_RECOVERY_METHOD, enter_recovery_rpc, NULL);
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }

    const esp_iris_screen_backend_t screen = {
        .begin = screen_begin,
        .read = screen_read,
        .end = screen_end,
        .user_ctx = &s_iris,
    };
    err = esp_iris_screen_register(&screen);
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }

    err = esp_iris_rpc_register(
        POINTER_SERVICE_ID, POINTER_METHOD_ID, pointer_rpc, &s_iris);
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }

    err = esp_iris_start();
    if (err != ESP_OK) {
        unregister_prestart();
        return err;
    }

    s_iris.started = true;
    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_app_desc_t *app = esp_app_get_description();
    ESP_LOGI(TAG,
             "IRIS_READY version=%s mode=normal writer=0 execution=recovery "
             "partition=%s",
             app ? app->version : "unknown",
             running ? running->label : "unknown");
    return ESP_OK;
}

esp_err_t esp_mosaico_iris_mark_healthy(void)
{
    if (!s_iris.started) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_iris_mark_healthy();
    if (err == ESP_OK) {
        const esp_partition_t *running = esp_ota_get_running_partition();
        ESP_LOGI(TAG, "IRIS_HEALTHY partition=%s",
                 running ? running->label : "unknown");
    }
    return err;
}

esp_err_t esp_mosaico_iris_stop(void)
{
    if (!s_iris.started) {
        unregister_prestart();
        memset(&s_iris, 0, sizeof(s_iris));
        return ESP_OK;
    }

    esp_err_t err = esp_iris_stop();
    if (err != ESP_OK) {
        return err;
    }

    esp_err_t inventory_err = esp_iris_system_inventory_unregister(NULL);
    if (inventory_err != ESP_OK && inventory_err != ESP_ERR_INVALID_STATE) {
        return inventory_err;
    }

    heap_caps_free(s_iris.capture);
    memset(&s_iris, 0, sizeof(s_iris));
    return ESP_OK;
}
