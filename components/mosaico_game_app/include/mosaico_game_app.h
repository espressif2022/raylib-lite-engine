// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_gsp.h"
#include "mosaico_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *tag;
    const char *window_title;
    uint16_t canvas_bind;
    uint8_t touch_points;
    bool enable_imu;
    uint16_t imu_sample_ms;
    int target_fps;
    uint16_t drawbuf_lines;
    uint8_t te_compose_buffers;
    uint32_t stats_interval;
    esp_gsp_config_t (*gsp_bundle)(void);
    esp_err_t (*register_mirror)(void);
    esp_err_t (*before_display)(void);
    esp_err_t (*on_start)(void);
    esp_err_t (*after_healthy)(void);
    void (*on_event)(const mosaico_device_event_t *event);
    bool (*idle)(void);
    void (*on_update)(void);
    void (*on_render)(void);
    void (*on_stats)(void);
} mosaico_game_app_config_t;

esp_err_t mosaico_game_app_run(const mosaico_game_app_config_t *config);

#ifdef __cplusplus
}
#endif
