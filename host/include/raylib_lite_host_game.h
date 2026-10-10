// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAYLIB_LITE_HOST_GAME_ABI_V1 1U

typedef enum {
    RAYLIB_LITE_HOST_INPUT_ACTION = 1,
    RAYLIB_LITE_HOST_INPUT_POINTER = 2,
    RAYLIB_LITE_HOST_INPUT_CONTROL = 3,
    RAYLIB_LITE_HOST_INPUT_IMU = 4,
} raylib_lite_host_input_type_t;

typedef enum {
    RAYLIB_LITE_HOST_CONTROL_PAUSE = 1,
    RAYLIB_LITE_HOST_CONTROL_RESUME,
    RAYLIB_LITE_HOST_CONTROL_RESET,
} raylib_lite_host_control_t;

typedef struct {
    uint32_t type;
    int32_t code;
    int32_t x;
    int32_t y;
    int32_t track_id;
    bool pressed;
    float value_x;
    float value_y;
    float value_z;
} raylib_lite_host_input_v1_t;

typedef struct {
    uint32_t abi_version;
    const char *game_id;
    const char *title;
    uint16_t width;
    uint16_t height;
    uint16_t tick_hz;
    uint16_t max_pointers;
} raylib_lite_host_game_descriptor_v1_t;

const raylib_lite_host_game_descriptor_v1_t *raylib_lite_host_game_v1(void);
void *raylib_lite_host_game_create_v1(const char *asset_root);
void raylib_lite_host_game_destroy_v1(void *game);
void raylib_lite_host_game_input_v1(void *game, const raylib_lite_host_input_v1_t *input);
void raylib_lite_host_game_update_v1(void *game);
int raylib_lite_host_game_render_rgb565_v1(void *game, uint16_t *pixels,
                                           size_t stride_pixels);
uint32_t raylib_lite_host_game_state_hash_v1(void *game);
int raylib_lite_host_game_state_json_v1(void *game, char *output, size_t capacity);
