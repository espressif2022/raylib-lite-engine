// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_input.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RAYLIB_LITE_ACTION_LEFT 0
#define RAYLIB_LITE_ACTION_RIGHT 1
#define RAYLIB_LITE_ACTION_JUMP 2
#define RAYLIB_LITE_ACTION_PAUSE 3
#define RAYLIB_LITE_ACTION_RESTART 4
#define RAYLIB_LITE_ACTION_COUNT 8
#define RAYLIB_LITE_ACTION_CONTACT_CAPACITY 2
#define RAYLIB_LITE_ACTION_ZONE_CAPACITY 8

typedef struct {
    int32_t track_id;
    int32_t x;
    int32_t y;
    bool active;
} raylib_lite_input_contact_t;

typedef struct {
    int16_t x0, y0, x1, y1;
    uint8_t action;
} raylib_lite_action_zone_t;

/* Shared action state is updated on the runner thread; callers must serialize
 * all access. Touch zones, buttons, joystick and IMU each own their held state.
 * Their logical OR determines an action, so a neutral sample or release from
 * one source cannot cancel another source. Opposing actions can both be held.
 * Touch and pointer events never imply a restart: bind a zone or button
 * explicitly. Invalid button actions are ignored. */
void raylib_lite_action_reset(void);
/* Cancel all current contacts, held input and pending/published edges without
 * synthesizing releases. Bindings and threshold remain configured. This takes
 * effect immediately, including between begin_frame calls. DETACHED applies
 * the same cancellation to the whole stream (events have no source identity).
 * Application-owned gesture state must also be cancelled by its event owner. */
void raylib_lite_action_cancel(void);
/* Replacing or clearing zones immediately remaps active contacts. */
void raylib_lite_action_set_zones(const raylib_lite_action_zone_t *zones, size_t count);
void raylib_lite_action_set_axis_threshold(int32_t threshold);
/* Publish and consume pending edges before each logic update, after applying
 * its input events. Queries remain stable until the next begin_frame. Edges
 * latch aggregate transitions, including a complete press/release between
 * ticks: pressed and released can both be true while down is false. Multiple
 * identical edges in one tick coalesce; repeated held samples add no edge. */
void raylib_lite_action_begin_frame(void);
void raylib_lite_action_apply_event(const raylib_lite_input_event_t *event);
bool raylib_lite_action_down(int action);
bool raylib_lite_action_pressed(int action);
bool raylib_lite_action_released(int action);
raylib_lite_input_contact_t raylib_lite_action_contact(size_t index);
size_t raylib_lite_action_contact_count(void);

#ifdef __cplusplus
}
#endif
