// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MOSAICO_SAVE_MAX_PAYLOAD 256
typedef esp_err_t (*mosaico_save_migrate_cb_t)(uint16_t old_version,const void *old_data,size_t old_size,void *new_data,size_t new_size);
typedef struct { const char *nvs_namespace;const char *key;uint16_t version;size_t payload_size;uint32_t debounce_ms;mosaico_save_migrate_cb_t migrate; } mosaico_save_config_t;
typedef struct { mosaico_save_config_t config;uint8_t pending[MOSAICO_SAVE_MAX_PAYLOAD];uint64_t due_ms;bool dirty,initialized; } mosaico_save_t;
esp_err_t mosaico_save_init(mosaico_save_t *save,const mosaico_save_config_t *config);
esp_err_t mosaico_save_load(mosaico_save_t *save,void *payload,const void *defaults);
esp_err_t mosaico_save_request(mosaico_save_t *save,const void *payload,uint64_t now_ms);
esp_err_t mosaico_save_flush(mosaico_save_t *save,uint64_t now_ms,bool force);
#ifdef __cplusplus
}
#endif
