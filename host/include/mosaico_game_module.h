// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "mosaico_host_game.h"

/* A project implements one Host descriptor that delegates to the same gameplay
 * and view functions compiled by the ESP-IDF application. */
typedef struct {
    mosaico_host_game_descriptor_v1_t descriptor;
    size_t state_size;
    int (*initialize)(void *state, const char *asset_root);
    void (*shutdown)(void *state);
    void (*input)(void *state, const mosaico_host_input_v1_t *input);
    void (*update)(void *state);
    int (*render)(void *state);
    uint32_t (*state_hash)(const void *state);
    int (*state_json)(const void *state, char *output, size_t capacity);
} mosaico_game_module_v1_t;

const mosaico_game_module_v1_t *mosaico_game_module_v1(void);
