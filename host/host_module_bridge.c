// SPDX-License-Identifier: Apache-2.0
#include <stdlib.h>
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"

typedef struct {
    const mosaico_game_module_v1_t *module;
    void *state;
} module_instance_t;

const mosaico_host_game_descriptor_v1_t *mosaico_host_game_v1(void)
{
    const mosaico_game_module_v1_t *module = mosaico_game_module_v1();
    return module ? &module->descriptor : NULL;
}

void *mosaico_host_game_create_v1(const char *asset_root)
{
    const mosaico_game_module_v1_t *module = mosaico_game_module_v1();
    if (!module || module->descriptor.abi_version != MOSAICO_HOST_GAME_ABI_V1 ||
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

void mosaico_host_game_destroy_v1(void *value)
{
    module_instance_t *instance = value;
    if (!instance) return;
    if (instance->module->shutdown) instance->module->shutdown(instance->state);
    free(instance->state);
    free(instance);
}

void mosaico_host_game_input_v1(void *value, const mosaico_host_input_v1_t *input)
{
    module_instance_t *instance = value;
    if (input) {
        if (input->type == MOSAICO_HOST_INPUT_ACTION)
            MosaicoFastInjectAction(input->code, input->pressed);
        else if (input->type == MOSAICO_HOST_INPUT_POINTER)
            MosaicoFastInjectPointer(input->track_id, input->x, input->y,
                                     input->pressed);
        else if (input->type == MOSAICO_HOST_INPUT_IMU)
            MosaicoFastInjectImu(input->value_x, input->value_y, input->value_z);
    }
    if (instance && instance->module->input)
        instance->module->input(instance->state, input);
}

void mosaico_host_game_update_v1(void *value)
{
    module_instance_t *instance = value;
    if (instance) instance->module->update(instance->state);
}

int mosaico_host_game_render_rgb565_v1(void *value, uint16_t *pixels,
                                       size_t stride_pixels)
{
    module_instance_t *instance = value;
    if (!instance || !pixels || stride_pixels < instance->module->descriptor.width)
        return -1;
    mosaico_host_raylib_set_target(pixels, stride_pixels,
        instance->module->descriptor.width, instance->module->descriptor.height);
    int result = instance->module->render(instance->state);
    mosaico_host_raylib_clear_target();
    return result;
}

uint32_t mosaico_host_game_state_hash_v1(void *value)
{
    module_instance_t *instance = value;
    return instance && instance->module->state_hash
        ? instance->module->state_hash(instance->state) : 0;
}

int mosaico_host_game_state_json_v1(void *value, char *output, size_t capacity)
{
    module_instance_t *instance = value;
    return instance && instance->module->state_json
        ? instance->module->state_json(instance->state, output, capacity) : -1;
}
