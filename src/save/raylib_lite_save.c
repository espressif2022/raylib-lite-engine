// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_save.h"

#include <string.h>

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t crc;
    uint8_t data[RAYLIB_LITE_SAVE_MAX_PAYLOAD];
} save_blob_t;

#define SAVE_MAGIC UINT32_C(0x3156534d)
#define SAVE_HEADER_SIZE offsetof(save_blob_t, data)

static uint32_t crc32(const void *data, size_t size)
{
    uint32_t crc = ~0U;
    const uint8_t *p = data;
    while (size--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xedb88320U & -(int32_t)(crc & 1));
    }
    return ~crc;
}

raylib_lite_result_t raylib_lite_save_init(
    raylib_lite_save_t *save, const raylib_lite_save_config_t *config)
{
    if (!save || !config || !config->storage || !config->storage->read ||
            !config->storage->write || !config->storage_namespace ||
            !config->key || !config->payload_size ||
            config->payload_size > RAYLIB_LITE_SAVE_MAX_PAYLOAD) {
        return RAYLIB_LITE_INVALID_ARGUMENT;
    }
    memset(save, 0, sizeof(*save));
    save->config = *config;
    save->initialized = true;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_save_load(
    raylib_lite_save_t *save, void *payload, const void *defaults)
{
    if (!save || !save->initialized || !payload) return RAYLIB_LITE_INVALID_STATE;
    if (defaults) memcpy(payload, defaults, save->config.payload_size);
    else memset(payload, 0, save->config.payload_size);

    save_blob_t blob;
    size_t size = sizeof(blob);
    raylib_lite_result_t error = save->config.storage->read(
        save->config.storage->context, save->config.storage_namespace,
        save->config.key, &blob, &size);
    if (error == RAYLIB_LITE_NOT_FOUND) return RAYLIB_LITE_OK;
    if (error != RAYLIB_LITE_OK) return error;
    if (size < SAVE_HEADER_SIZE || blob.size > RAYLIB_LITE_SAVE_MAX_PAYLOAD ||
            SAVE_HEADER_SIZE + blob.size != size) {
        return RAYLIB_LITE_INVALID_CRC;
    }
    if (blob.magic != SAVE_MAGIC || crc32(blob.data, blob.size) != blob.crc)
        return RAYLIB_LITE_INVALID_CRC;
    if (blob.version == save->config.version &&
            blob.size == save->config.payload_size) {
        memcpy(payload, blob.data, blob.size);
        return RAYLIB_LITE_OK;
    }
    return save->config.migrate
        ? save->config.migrate(blob.version, blob.data, blob.size, payload,
                               save->config.payload_size)
        : RAYLIB_LITE_INVALID_VERSION;
}

raylib_lite_result_t raylib_lite_save_request(
    raylib_lite_save_t *save, const void *payload, uint64_t now_ms)
{
    if (!save || !save->initialized || !payload) return RAYLIB_LITE_INVALID_ARGUMENT;
    memcpy(save->pending, payload, save->config.payload_size);
    save->due_ms = now_ms + save->config.debounce_ms;
    save->dirty = true;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_save_flush(raylib_lite_save_t *save, uint64_t now_ms, bool force)
{
    if (!save || !save->initialized) return RAYLIB_LITE_INVALID_STATE;
    if (!save->dirty || (!force && now_ms < save->due_ms)) return RAYLIB_LITE_OK;

    save_blob_t blob = {
        .magic = SAVE_MAGIC,
        .version = save->config.version,
        .size = (uint16_t)save->config.payload_size,
        .crc = crc32(save->pending, save->config.payload_size),
    };
    memcpy(blob.data, save->pending, save->config.payload_size);
    raylib_lite_result_t error = save->config.storage->write(
        save->config.storage->context, save->config.storage_namespace,
        save->config.key, &blob, SAVE_HEADER_SIZE + save->config.payload_size);
    if (error == RAYLIB_LITE_OK) save->dirty = false;
    return error;
}
