// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_iris.h"

#include <stdlib.h>
#include <string.h>
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_iris_system_inventory.h"
#include "esp_partition.h"
#include "psa/crypto.h"
#include "sdkconfig.h"

#define INVENTORY_HASH_CHUNK_BYTES 1024U
#define SYSTEM_METADATA_MAGIC 0x49535953U
#define SYSTEM_METADATA_VERSION 1U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    int32_t result;
    uint8_t reserved[36];
} mosaico_system_metadata_t;

static esp_err_t inventory_hash_flash(uint32_t address, size_t size,
                                      uint8_t output[32])
{
    uint8_t *buffer = heap_caps_malloc(INVENTORY_HASH_CHUNK_BYTES,
                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buffer) return ESP_ERR_NO_MEM;
    psa_hash_operation_t hash = PSA_HASH_OPERATION_INIT;
    esp_err_t err = ESP_OK;
    if (psa_crypto_init() != PSA_SUCCESS ||
        psa_hash_setup(&hash, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        free(buffer);
        return ESP_FAIL;
    }
    for (size_t offset = 0; offset < size;) {
        size_t chunk = size - offset;
        if (chunk > INVENTORY_HASH_CHUNK_BYTES) chunk = INVENTORY_HASH_CHUNK_BYTES;
        if (esp_flash_read(NULL, buffer, address + offset, chunk) != ESP_OK ||
            psa_hash_update(&hash, buffer, chunk) != PSA_SUCCESS) {
            err = ESP_FAIL;
            break;
        }
        offset += chunk;
    }
    size_t written = 0;
    if (err == ESP_OK &&
        (psa_hash_finish(&hash, output, 32, &written) != PSA_SUCCESS ||
         written != 32)) err = ESP_FAIL;
    if (err != ESP_OK) (void)psa_hash_abort(&hash);
    free(buffer);
    return err;
}

static esp_err_t game_inventory_get(esp_iris_system_inventory_t *inventory,
                                    void *user_ctx)
{
    (void)user_ctx;
    if (!inventory) return ESP_ERR_INVALID_ARG;
    memset(inventory, 0, sizeof(*inventory));
    inventory->layout_version = 3;
    esp_err_t err = inventory_hash_flash(CONFIG_BOOTLOADER_OFFSET_IN_FLASH,
        CONFIG_PARTITION_TABLE_OFFSET - CONFIG_BOOTLOADER_OFFSET_IN_FLASH,
        inventory->bootloader_sha256);
    if (err != ESP_OK) return err;
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_BOOTLOADER_SHA256;
    err = inventory_hash_flash(CONFIG_PARTITION_TABLE_OFFSET, 0x1000,
                               inventory->partition_table_sha256);
    if (err != ESP_OK) return err;
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_PARTITION_TABLE_SHA256;
    const esp_partition_t *sysmeta = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "sysmeta");
    mosaico_system_metadata_t record;
    if (sysmeta && sysmeta->size >= sizeof(record) &&
        esp_partition_read(sysmeta, 0, &record, sizeof(record)) == ESP_OK &&
        record.magic == SYSTEM_METADATA_MAGIC &&
        record.version == SYSTEM_METADATA_VERSION) {
        memcpy(inventory->last_operation_id, record.operation_id,
               sizeof(inventory->last_operation_id));
        inventory->last_result = record.result;
        inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_LAST_OPERATION;
    }
    return ESP_OK;
}

esp_err_t mosaico_game_iris_register_inventory(void)
{
    const esp_iris_system_inventory_provider_t provider = {
        .get_inventory = game_inventory_get,
        .user_ctx = NULL,
    };
    return esp_iris_system_inventory_register(&provider);
}
