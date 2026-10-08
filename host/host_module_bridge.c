// SPDX-License-Identifier: Apache-2.0
#include <stdlib.h>

#include "raylib_lite_host_game.h"
#include "raylib_lite_game_module.h"
#include "raylib_lite_raylib_impl.h"
#include "raylib_lite_host_video.h"

_Static_assert(RAYLIB_LITE_HOST_GAME_ABI_V1 == RAYLIB_LITE_GAME_MODULE_ABI_V1,
               "Host and example module ABI versions must match");
/* The Host and Game constants belong to distinct anonymous enums.
 * Compare their integer values without GCC's -Wenum-compare warning. */
_Static_assert((int)RAYLIB_LITE_HOST_INPUT_ACTION == (int)RAYLIB_LITE_GAME_INPUT_ACTION,
               "Host and example action input values must match");
_Static_assert((int)RAYLIB_LITE_HOST_INPUT_POINTER == (int)RAYLIB_LITE_GAME_INPUT_POINTER,
               "Host and example pointer input values must match");
_Static_assert((int)RAYLIB_LITE_HOST_INPUT_CONTROL == (int)RAYLIB_LITE_GAME_INPUT_CONTROL,
               "Host and example control input values must match");
_Static_assert((int)RAYLIB_LITE_HOST_INPUT_IMU == (int)RAYLIB_LITE_GAME_INPUT_IMU,
               "Host and example IMU input values must match");

typedef struct {
    const raylib_lite_game_module_v1_t *module;
    void *state;
} module_instance_t;

static raylib_lite_host_game_descriptor_v1_t s_host_descriptor;

const raylib_lite_host_game_descriptor_v1_t *raylib_lite_host_game_v1(void)
{
    const raylib_lite_game_module_v1_t *module = raylib_lite_game_module_v1();
    if (!module) return NULL;

    s_host_descriptor = (raylib_lite_host_game_descriptor_v1_t) {
        .abi_version = module->descriptor.abi_version,
        .game_id = module->descriptor.game_id,
        .title = module->descriptor.title,
        .width = module->descriptor.width,
        .height = module->descriptor.height,
        .tick_hz = module->descriptor.tick_hz,
        .max_pointers = module->descriptor.max_pointers,
    };
    return &s_host_descriptor;
}

void *raylib_lite_host_game_create_v1(const char *asset_root)
{
    const raylib_lite_game_module_v1_t *module = raylib_lite_game_module_v1();
    if (!module || module->descriptor.abi_version != RAYLIB_LITE_GAME_MODULE_ABI_V1 ||
            !module->state_size || !module->update || !module->render)
        return NULL;
    module_instance_t *instance = calloc(1, sizeof(*instance));
    if (!instance) return NULL;
    instance->state = calloc(1, module->state_size);
    if (!instance->state) { free(instance); return NULL; }
    instance->module = module;
    if (module->initialize && module->initialize(instance->state, asset_root)) {
        free(instance->state); free(instance); return NULL;
    }
    return instance;
}

void raylib_lite_host_game_destroy_v1(void *value)
{
    module_instance_t *instance = value;
    if (!instance) return;
    if (instance->module->shutdown) instance->module->shutdown(instance->state);
    raylib_lite_host_video_shutdown();
    free(instance->state);
    free(instance);
}

void raylib_lite_host_game_input_v1(void *value,
                                    const raylib_lite_host_input_v1_t *input)
{
    module_instance_t *instance = value;
    if (input) {
        if (input->type == RAYLIB_LITE_HOST_INPUT_ACTION)
            raylib_lite_raylib_inject_action(input->code, input->pressed);
        else if (input->type == RAYLIB_LITE_HOST_INPUT_POINTER)
            raylib_lite_raylib_inject_pointer(input->track_id, input->x, input->y,
                                              input->pressed);
        else if (input->type == RAYLIB_LITE_HOST_INPUT_IMU)
            raylib_lite_raylib_inject_imu(input->value_x, input->value_y,
                                          input->value_z);
    }

    if (!instance || !instance->module->input) return;
    if (!input) {
        instance->module->input(instance->state, NULL);
        return;
    }

    const raylib_lite_game_input_v1_t game_input = {
        .type = input->type,
        .code = input->code,
        .x = input->x,
        .y = input->y,
        .track_id = input->track_id,
        .pressed = input->pressed,
        .value_x = input->value_x,
        .value_y = input->value_y,
        .value_z = input->value_z,
    };
    instance->module->input(instance->state, &game_input);
}

void raylib_lite_host_game_update_v1(void *value)
{
    module_instance_t *instance = value;
    if (instance) instance->module->update(instance->state);
}

int raylib_lite_host_game_render_rgb565_v1(void *value, uint16_t *pixels,
                                           size_t stride_pixels)
{
    module_instance_t *instance = value;
    if (!instance || !pixels || stride_pixels < instance->module->descriptor.width)
        return -1;
    if (raylib_lite_host_video_set_target(
            pixels, stride_pixels, instance->module->descriptor.width,
            instance->module->descriptor.height) != RAYLIB_LITE_OK) {
        return -1;
    }
    int result = instance->module->render(instance->state);
    raylib_lite_host_video_clear_target();
    return result;
}

uint32_t raylib_lite_host_game_state_hash_v1(void *value)
{
    module_instance_t *instance = value;
    return instance && instance->module->state_hash
        ? instance->module->state_hash(instance->state) : 0;
}

int raylib_lite_host_game_state_json_v1(void *value, char *output, size_t capacity)
{
    module_instance_t *instance = value;
    return instance && instance->module->state_json
        ? instance->module->state_json(instance->state, output, capacity) : -1;
}
