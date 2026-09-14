// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MOSAICO_HOST_GAME_ABI_V1 1U

typedef enum {
    MOSAICO_HOST_INPUT_ACTION = 1,
    MOSAICO_HOST_INPUT_POINTER = 2,
    MOSAICO_HOST_INPUT_CONTROL = 3,
    MOSAICO_HOST_INPUT_IMU = 4,
} mosaico_host_input_type_t;

typedef enum {
    MOSAICO_HOST_CONTROL_PAUSE = 1,
    MOSAICO_HOST_CONTROL_RESUME,
    MOSAICO_HOST_CONTROL_RESET,
} mosaico_host_control_t;

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
} mosaico_host_input_v1_t;

typedef struct {
    uint32_t abi_version;
    const char *game_id;
    const char *title;
    uint16_t width;
    uint16_t height;
    uint16_t tick_hz;
    uint16_t max_pointers;
} mosaico_host_game_descriptor_v1_t;

const mosaico_host_game_descriptor_v1_t *mosaico_host_game_v1(void);
void *mosaico_host_game_create_v1(const char *asset_root);
void mosaico_host_game_destroy_v1(void *game);
void mosaico_host_game_input_v1(void *game, const mosaico_host_input_v1_t *input);
void mosaico_host_game_update_v1(void *game);
int mosaico_host_game_render_rgb565_v1(void *game, uint16_t *pixels,
                                       size_t stride_pixels);
uint32_t mosaico_host_game_state_hash_v1(void *game);
int mosaico_host_game_state_json_v1(void *game, char *output, size_t capacity);
