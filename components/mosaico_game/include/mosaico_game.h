// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAICO_GAME_API_VERSION 3U
#define MOSAICO_GAME_WIDTH 480
#define MOSAICO_GAME_HEIGHT 480
#ifndef MOSAICO_GAME_DEFAULT_FPS
#ifdef CONFIG_MOSAICO_GAME_DEFAULT_FPS
#define MOSAICO_GAME_DEFAULT_FPS CONFIG_MOSAICO_GAME_DEFAULT_FPS
#else
#define MOSAICO_GAME_DEFAULT_FPS 30
#endif
#endif

typedef struct {
    int width;
    int height;
    int target_fps;
    size_t game_task_stack;
} mosaico_game_config_t;

#define MOSAICO_GAME_CONFIG_DEFAULT() { \
    .width = MOSAICO_GAME_WIDTH, \
    .height = MOSAICO_GAME_HEIGHT, \
    .target_fps = MOSAICO_GAME_DEFAULT_FPS, \
    .game_task_stack = 12288U, \
}

typedef enum {
    MOSAICO_DEVICE_EVENT_NONE = 0,
    MOSAICO_DEVICE_EVENT_POINTER,
    MOSAICO_DEVICE_EVENT_TOUCH,
    MOSAICO_DEVICE_EVENT_BUTTON,
    MOSAICO_DEVICE_EVENT_JOYSTICK,
    MOSAICO_DEVICE_EVENT_IMU,
    MOSAICO_DEVICE_EVENT_ATTACHED,
    MOSAICO_DEVICE_EVENT_DETACHED,
} mosaico_device_event_type_t;

typedef struct {
    mosaico_device_event_type_t type;
    int32_t x;
    int32_t y;
    int32_t value;
    bool pressed;
    uint64_t timestamp_us;
} mosaico_device_event_t;

typedef struct {
    uint32_t frames;
    uint32_t dropped_frames;
    float fps;
    float logic_fps;
    float display_fps;
    uint32_t input_us;
    uint32_t update_us;
    uint32_t acquire_us;
    uint32_t render_us;
    uint32_t present_us;
    uint32_t release_us;
    uint32_t busy_frames;
    uint32_t superseded_frames;
    uint32_t display_errors;
    uint32_t queue_overflows;
    uint32_t in_flight_frames;
    uint32_t peak_in_flight_frames;
    size_t free_internal_bytes;
    size_t free_psram_bytes;
} mosaico_game_stats_t;

typedef enum {
    MOSAICO_GAME_FRAME_ACCEPTED = 0,
    MOSAICO_GAME_FRAME_BUSY,
    MOSAICO_GAME_FRAME_SUPERSEDED,
    MOSAICO_GAME_FRAME_DISPLAY_ERROR,
} mosaico_game_frame_result_t;

esp_err_t MosaicoGameInit(const mosaico_game_config_t *config);
void MosaicoGameShutdown(void);
bool MosaicoGamePollDeviceEvent(mosaico_device_event_t *event);
bool MosaicoGamePostDeviceEvent(const mosaico_device_event_t *event);
void MosaicoGameGetStats(mosaico_game_stats_t *stats);
void MosaicoGameRecordFrame(uint32_t update_us, uint32_t render_us,
                            uint32_t present_us, bool dropped);
void MosaicoGameRecordTiming(uint32_t update_us, uint32_t render_us);
void MosaicoGameRecordLogic(uint32_t input_us, uint32_t update_us);
void MosaicoGameRecordRender(uint32_t acquire_us, uint32_t render_us,
                             uint32_t present_us,
                             mosaico_game_frame_result_t result,
                             uint32_t in_flight_frames);
void MosaicoGameRecordDisplayRelease(uint32_t release_us,
                                     uint32_t in_flight_frames);
const mosaico_game_config_t *MosaicoGameGetConfig(void);

#ifdef __cplusplus
}
#endif
