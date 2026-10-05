// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>

#include "mosaico_game_action.h"

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
    const mosaico_action_zone_t zones[] = {
        {.x0 = 0, .y0 = 0, .x1 = 100, .y1 = 100,
         .action = MOSAICO_ACTION_LEFT},
    };
    mosaico_action_reset();
    mosaico_action_set_zones(zones, 1);

    raylib_lite_input_event_t e = event(
        RAYLIB_LITE_INPUT_TOUCH, 20, 30, 7, true);
    mosaico_action_apply_event(&e);
    mosaico_action_begin_frame();
    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_pressed(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_pressed(MOSAICO_ACTION_RESTART));
    assert(mosaico_action_contact_count() == 1);

    mosaico_action_begin_frame();
    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
    assert(!mosaico_action_pressed(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_released(MOSAICO_ACTION_RESTART));

    e = event(RAYLIB_LITE_INPUT_TOUCH, 20, 30, 7, false);
    mosaico_action_apply_event(&e);
    mosaico_action_begin_frame();
    assert(!mosaico_action_down(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_released(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_contact_count() == 0);

    e = event(RAYLIB_LITE_INPUT_IMU, -500, 0, 0, false);
    mosaico_action_apply_event(&e);
    mosaico_action_begin_frame();
    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
    assert(!mosaico_action_down(MOSAICO_ACTION_RIGHT));

    e = event(RAYLIB_LITE_INPUT_IMU, 500, 0, 0, false);
    mosaico_action_apply_event(&e);
    mosaico_action_begin_frame();
    assert(!mosaico_action_down(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_released(MOSAICO_ACTION_LEFT));
    assert(mosaico_action_down(MOSAICO_ACTION_RIGHT));
    assert(mosaico_action_pressed(MOSAICO_ACTION_RIGHT));

    puts("game action: ok");
    return 0;
}
