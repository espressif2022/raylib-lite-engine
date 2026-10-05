// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mosaico_game_save.h"

typedef struct {
    uint8_t bytes[512];
    size_t size;
    int present;
    int writes;
    int fail_write;
} fake_store_t;

static esp_err_t fake_read(void *context, const char *namespace_name,
                           const char *key, void *data, size_t *size)
{
    fake_store_t *store = context;
    assert(strcmp(namespace_name, "test") == 0);
    assert(strcmp(key, "slot") == 0);
    if (!store->present) return ESP_ERR_NOT_FOUND;
    if (*size < store->size) return ESP_ERR_INVALID_SIZE;
    memcpy(data, store->bytes, store->size);
    *size = store->size;
    return ESP_OK;
}

static esp_err_t fake_write(void *context, const char *namespace_name,
                            const char *key, const void *data, size_t size)
{
    fake_store_t *store = context;
    assert(strcmp(namespace_name, "test") == 0);
    assert(strcmp(key, "slot") == 0);
    if (store->fail_write) return ESP_FAIL;
    assert(size <= sizeof(store->bytes));
    memcpy(store->bytes, data, size);
    store->size = size;
    store->present = 1;
    ++store->writes;
    return ESP_OK;
}

typedef struct { uint32_t value; uint32_t extra; } payload_v2_t;

static esp_err_t migrate_v1(uint16_t version, const void *old_data,
                            size_t old_size, void *new_data, size_t new_size)
{
    assert(version == 1);
    assert(old_size == sizeof(uint32_t));
    assert(new_size == sizeof(payload_v2_t));
    payload_v2_t *out = new_data;
    out->value = *(const uint32_t *)old_data;
    out->extra = 77;
    return ESP_OK;
}

int main(void)
{
    fake_store_t store = {0};
    const mosaico_save_storage_t storage = {
        .context = &store,
        .read = fake_read,
        .write = fake_write,
    };
    mosaico_save_t save;
    mosaico_save_config_t config = {
        .storage = &storage,
        .storage_namespace = "test",
        .key = "slot",
        .version = 2,
        .payload_size = sizeof(payload_v2_t),
        .debounce_ms = 100,
    };
    assert(mosaico_save_init(&save, &config) == ESP_OK);

    payload_v2_t defaults = {.value = 1, .extra = 2};
    payload_v2_t out = {0};
    assert(mosaico_save_load(&save, &out, &defaults) == ESP_OK);
    assert(out.value == 1 && out.extra == 2);

    payload_v2_t value = {.value = 10, .extra = 20};
    assert(mosaico_save_request(&save, &value, 1000) == ESP_OK);
    assert(mosaico_save_flush(&save, 1099, false) == ESP_OK);
    assert(store.writes == 0 && save.dirty);
    assert(mosaico_save_flush(&save, 1100, false) == ESP_OK);
    assert(store.writes == 1 && !save.dirty);
    memset(&out, 0, sizeof(out));
    assert(mosaico_save_load(&save, &out, NULL) == ESP_OK);
    assert(out.value == 10 && out.extra == 20);

    store.bytes[store.size - 1] ^= 0x55;
    assert(mosaico_save_load(&save, &out, NULL) == ESP_ERR_INVALID_CRC);

    mosaico_save_t old_save;
    mosaico_save_config_t old_config = config;
    old_config.version = 1;
    old_config.payload_size = sizeof(uint32_t);
    old_config.migrate = NULL;
    assert(mosaico_save_init(&old_save, &old_config) == ESP_OK);
    uint32_t old_value = 42;
    assert(mosaico_save_request(&old_save, &old_value, 0) == ESP_OK);
    assert(mosaico_save_flush(&old_save, 0, true) == ESP_OK);

    mosaico_save_t new_save;
    config.migrate = migrate_v1;
    assert(mosaico_save_init(&new_save, &config) == ESP_OK);
    memset(&out, 0, sizeof(out));
    assert(mosaico_save_load(&new_save, &out, NULL) == ESP_OK);
    assert(out.value == 42 && out.extra == 77);

    store.fail_write = 1;
    value.value = 99;
    assert(mosaico_save_request(&new_save, &value, 0) == ESP_OK);
    assert(mosaico_save_flush(&new_save, 0, true) == ESP_FAIL);
    assert(new_save.dirty);

    puts("game save: ok");
    return 0;
}
