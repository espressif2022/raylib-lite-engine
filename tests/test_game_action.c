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

static const raylib_lite_action_zone_t direction_zones[] = {
    {0, 0, 100, 100, RAYLIB_LITE_ACTION_LEFT},
    {100, 0, 200, 100, RAYLIB_LITE_ACTION_RIGHT},
};

static void apply(raylib_lite_input_type_t type, int x, int value, bool pressed)
{
    raylib_lite_input_event_t e = event(type, x, 20, value, pressed);
    raylib_lite_action_apply_event(&e);
}

static void assert_action(int action, bool down, bool pressed, bool released)
{
    assert(raylib_lite_action_down(action) == down);
    assert(raylib_lite_action_pressed(action) == pressed);
    assert(raylib_lite_action_released(action) == released);
}

static void test_source_aggregation(void)
{
    const raylib_lite_input_type_t sources[] = {
        RAYLIB_LITE_INPUT_TOUCH, RAYLIB_LITE_INPUT_BUTTON,
        RAYLIB_LITE_INPUT_JOYSTICK, RAYLIB_LITE_INPUT_IMU,
    };
    /* Any held source survives neutral samples from all other sources in
     * either order. Every source also releases cleanly on its own. */
    for (int held_source = 0; held_source < 4; ++held_source) {
        for (int reverse = 0; reverse < 2; ++reverse) {
            raylib_lite_action_reset();
            raylib_lite_action_set_zones(direction_zones, 2);
            for (int n = 0; n < 4; ++n) {
                int i = reverse ? 3 - n : n;
                bool held = i == held_source;
                int x = i < 2 ? 20 : (held ? -500 : 0);
                apply(sources[i], x, 0, held);
            }
            raylib_lite_action_begin_frame();
            assert_action(RAYLIB_LITE_ACTION_LEFT, true, true, false);
            apply(sources[held_source], held_source < 2 ? 20 : 0, 0, false);
            raylib_lite_action_begin_frame();
            assert_action(RAYLIB_LITE_ACTION_LEFT, false, false, true);
        }
    }

    raylib_lite_action_reset();
    raylib_lite_action_set_zones(direction_zones, 2);
    for (int i = 0; i < 4; ++i) apply(sources[i], i < 2 ? 20 : -500, 0, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, true, true, false);
    for (int i = 0; i < 4; ++i) {
        apply(sources[i], i < 2 ? 20 : 0, 0, false);
        raylib_lite_action_begin_frame();
        assert_action(RAYLIB_LITE_ACTION_LEFT, i != 3, false, i == 3);
    }
}

static void test_short_taps_and_moves(void)
{
    raylib_lite_action_reset();
    raylib_lite_action_set_zones(direction_zones, 2);
    /* Two clicks inside one tick coalesce without becoming a held action. */
    for (int i = 0; i < 2; ++i) {
        apply(RAYLIB_LITE_INPUT_TOUCH, 20, 7, true);
        apply(RAYLIB_LITE_INPUT_TOUCH, 20, 7, false);
    }
    assert_action(RAYLIB_LITE_ACTION_LEFT, false, false, false);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, false, true, true);
    assert_action(RAYLIB_LITE_ACTION_RESTART, false, false, false);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, false, false, false);

    apply(RAYLIB_LITE_INPUT_TOUCH, 20, 7, true);
    apply(RAYLIB_LITE_INPUT_TOUCH, 120, 7, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, false, true, true);
    assert_action(RAYLIB_LITE_ACTION_RIGHT, true, true, false);
    apply(RAYLIB_LITE_INPUT_TOUCH, 120, 7, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_RIGHT, true, false, false);
    apply(RAYLIB_LITE_INPUT_TOUCH, 120, 7, false);
    apply(RAYLIB_LITE_INPUT_TOUCH, 120, 7, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_RIGHT, true, true, true);

    apply(RAYLIB_LITE_INPUT_BUTTON, 0, RAYLIB_LITE_ACTION_JUMP, true);
    apply(RAYLIB_LITE_INPUT_BUTTON, 0, RAYLIB_LITE_ACTION_JUMP, false);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_JUMP, false, true, true);
}

static void test_contacts_and_zone_replacement(void)
{
    raylib_lite_action_reset();
    raylib_lite_action_set_zones(direction_zones, 2);
    apply(RAYLIB_LITE_INPUT_TOUCH, 20, 7, true);
    apply(RAYLIB_LITE_INPUT_TOUCH, 30, 8, true);
    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_contact_count() == 2);
    assert_action(RAYLIB_LITE_ACTION_LEFT, true, true, false);
    apply(RAYLIB_LITE_INPUT_TOUCH, 20, 7, false);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, true, false, false);
    apply(RAYLIB_LITE_INPUT_TOUCH, 120, 8, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, false, false, true);
    assert_action(RAYLIB_LITE_ACTION_RIGHT, true, true, false);

    const raylib_lite_action_zone_t restart = {
        0, 0, 200, 100, RAYLIB_LITE_ACTION_RESTART,
    };
    raylib_lite_action_set_zones(&restart, 1);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_RIGHT, false, false, true);
    assert_action(RAYLIB_LITE_ACTION_RESTART, true, true, false);
    raylib_lite_action_set_zones(NULL, 100);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_RESTART, false, false, true);

    /* A pointer and touch tracking ID zero are distinct contacts. */
    raylib_lite_action_reset();
    raylib_lite_action_set_zones(direction_zones, 2);
    apply(RAYLIB_LITE_INPUT_TOUCH, 20, 0, true);
    apply(RAYLIB_LITE_INPUT_POINTER, 120, 0, true);
    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_contact_count() == 2);
    assert_action(RAYLIB_LITE_ACTION_LEFT, true, true, false);
    assert_action(RAYLIB_LITE_ACTION_RIGHT, true, true, false);
    apply(RAYLIB_LITE_INPUT_POINTER, 120, 0, false);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_LEFT, true, false, false);
    assert_action(RAYLIB_LITE_ACTION_RIGHT, false, false, true);
    apply(RAYLIB_LITE_INPUT_BUTTON, 0, -1, true);
    apply(RAYLIB_LITE_INPUT_BUTTON, 0, RAYLIB_LITE_ACTION_COUNT, true);
    raylib_lite_action_begin_frame();
    assert_action(RAYLIB_LITE_ACTION_PAUSE, false, false, false);
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
    assert(!raylib_lite_action_pressed(RAYLIB_LITE_ACTION_RESTART));
    assert(raylib_lite_action_contact_count() == 1);

    raylib_lite_action_begin_frame();
    assert(raylib_lite_action_down(RAYLIB_LITE_ACTION_LEFT));
    assert(!raylib_lite_action_pressed(RAYLIB_LITE_ACTION_LEFT));
    assert(!raylib_lite_action_released(RAYLIB_LITE_ACTION_RESTART));

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

    test_source_aggregation();
    test_short_taps_and_moves();
    test_contacts_and_zone_replacement();
    puts("game action: ok");
    return 0;
}
