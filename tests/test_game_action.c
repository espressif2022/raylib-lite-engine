// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>

#include "raylib_lite_action.h"

static raylib_lite_input_event_t event(raylib_lite_input_type_t type,
                                       int32_t x, int32_t y, int32_t value,
                                       bool pressed)
{
    return (raylib_lite_input_event_t) {
        .type = type,
        .x = x,
        .y = y,
        .value = value,
        .pressed = pressed,
        .timestamp_us = 1,
    };
}

int main(void)
{
    const raylib_lite_action_zone_t zones[] = {
        {.x0 = 0, .y0 = 0, .x1 = 100, .y1 = 100,
         .action = RAYLIB_LITE_ACTION_LEFT},
    };
    raylib_lite_action_reset();
    raylib_lite_action_set_zones(zones, 1);

    raylib_lite_input_event_t e = event(
        RAYLIB_LITE_INPUT_TOUCH, 20, 30, 7, true);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_pressed(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_pressed(RAYLIB_LITE_ACTION_RESTART));
    assert(raylib_lite_action_contact_count() == 1);

    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(!raylib_lite_action_pressed(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_released(RAYLIB_LITE_ACTION_RESTART));

    e = event(RAYLIB_LITE_INPUT_TOUCH, 20, 30, 7, false);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(!raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_released(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_contact_count() == 0);

    e = event(RAYLIB_LITE_INPUT_IMU, -500, 0, 0, false);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(!raylib_lite_action_down(RAYLIB_LITE_ACTION_RIGHT));

    e = event(RAYLIB_LITE_INPUT_IMU, 500, 0, 0, false);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(!raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_released(RAYLIB_LITE_ACTION_LEFT));
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_RIGHT));
    assert(raylib_lite_action_pressed(RAYLIB_LITE_ACTION_RIGHT));

    /* Invalid zone actions are rejected before the touch hot path can use
     * them as array indices. Valid zones in the same update remain active. */
    const raylib_lite_action_zone_t mixed_zones[] = {
        {.x0 = 0, .y0 = 0, .x1 = 50, .y1 = 50,
         .action = RAYLIB_LITE_ACTION_COUNT},
        {.x0 = 50, .y0 = 0, .x1 = 100, .y1 = 50,
         .action = RAYLIB_LITE_ACTION_RIGHT},
    };
    raylib_lite_action_reset();
    raylib_lite_action_set_zones(mixed_zones, 2);
    e = event(RAYLIB_LITE_INPUT_TOUCH, 20, 20, 1, true);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(!raylib_lite_action_down(RAYLIB_LITE_ACTION_RIGHT));
    e = event(RAYLIB_LITE_INPUT_TOUCH, 20, 20, 1, false);
    raylib_lite_action_apply_event(&e);
    e = event(RAYLIB_LITE_INPUT_TOUCH, 70, 20, 2, true);
    raylib_lite_action_apply_event(&e);
    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_RIGHT));

    puts("game action: ok");
    return 0;
}
