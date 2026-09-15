// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mosaico_game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOSAICO_ACTION_LEFT 0
#define MOSAICO_ACTION_RIGHT 1
#define MOSAICO_ACTION_JUMP 2
#define MOSAICO_ACTION_PAUSE 3
#define MOSAICO_ACTION_RESTART 4
#define MOSAICO_ACTION_COUNT 8
#define MOSAICO_CONTACT_CAPACITY 2
#define MOSAICO_ACTION_ZONE_CAPACITY 8

typedef struct {
    int32_t track_id;
    int32_t x;
    int32_t y;
    bool active;
} mosaico_input_contact_t;

typedef struct {
    int16_t x0, y0, x1, y1;
    uint8_t action;
} mosaico_action_zone_t;

void mosaico_action_reset(void);
void mosaico_action_set_zones(const mosaico_action_zone_t *zones, size_t count);
void mosaico_action_set_axis_threshold(int32_t threshold);
void mosaico_action_begin_frame(void);
void mosaico_action_apply_event(const mosaico_device_event_t *event);
bool mosaico_action_down(int action);
bool mosaico_action_pressed(int action);
bool mosaico_action_released(int action);
mosaico_input_contact_t mosaico_action_contact(size_t index);
size_t mosaico_action_contact_count(void);

#ifdef __cplusplus
}
#endif
