// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_action.h"
#include <string.h>

static raylib_lite_input_contact_t s_contacts[RAYLIB_LITE_ACTION_CONTACT_CAPACITY];
static raylib_lite_input_type_t s_contact_types[RAYLIB_LITE_ACTION_CONTACT_CAPACITY];
static raylib_lite_action_zone_t s_zones[RAYLIB_LITE_ACTION_ZONE_CAPACITY];
static size_t s_zone_count;
static int32_t s_axis_threshold = 250;
enum action_source { SOURCE_ZONES, SOURCE_BUTTONS, SOURCE_JOYSTICK, SOURCE_IMU,
                     SOURCE_COUNT };
static bool s_sources[SOURCE_COUNT][RAYLIB_LITE_ACTION_COUNT];
static bool s_held[RAYLIB_LITE_ACTION_COUNT];
static bool s_pending_pressed[RAYLIB_LITE_ACTION_COUNT];
static bool s_pending_released[RAYLIB_LITE_ACTION_COUNT];
static bool s_down[RAYLIB_LITE_ACTION_COUNT];
static bool s_pressed[RAYLIB_LITE_ACTION_COUNT];
static bool s_released[RAYLIB_LITE_ACTION_COUNT];
/* Latch transitions as events arrive, before the runner drains the next
 * logic tick. A press followed by a release must survive the final held state. */
static void set_source(enum action_source source, int action, bool down)
{
    if ((unsigned)action >= RAYLIB_LITE_ACTION_COUNT) return;
    s_sources[source][action] = down;
    bool held = false;
    for (int i = 0; i < SOURCE_COUNT; ++i) held |= s_sources[i][action];
    if (held && !s_held[action]) s_pending_pressed[action] = true;
    if (!held && s_held[action]) s_pending_released[action] = true;
    s_held[action] = held;
}

static void sync_zones(void)
{
    bool down[RAYLIB_LITE_ACTION_COUNT] = {0};
    for (size_t z = 0; z < s_zone_count; ++z) {
        for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; ++i) {
            if (!s_contacts[i].active) continue;
            if (s_contacts[i].x >= s_zones[z].x0 && s_contacts[i].x < s_zones[z].x1 &&
                s_contacts[i].y >= s_zones[z].y0 && s_contacts[i].y < s_zones[z].y1)
                down[s_zones[z].action] = true;
        }
    }
    for (int action = 0; action < RAYLIB_LITE_ACTION_COUNT; ++action)
        set_source(SOURCE_ZONES, action, down[action]);
}

static raylib_lite_input_contact_t *contact_for(
    raylib_lite_input_type_t type, int32_t track_id, bool pressed)
{
    for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; ++i)
        if (s_contacts[i].active && s_contact_types[i] == type &&
                s_contacts[i].track_id == track_id)
            return &s_contacts[i];
    if (!pressed) return NULL;
    for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; ++i)
        if (!s_contacts[i].active) {
            s_contact_types[i] = type;
            return &s_contacts[i];
        }
    return NULL;
}

void raylib_lite_action_cancel(void)
{
    memset(s_contacts, 0, sizeof(s_contacts));
    memset(s_contact_types, 0, sizeof(s_contact_types));
    memset(s_sources, 0, sizeof(s_sources));
    memset(s_pending_pressed, 0, sizeof(s_pending_pressed));
    memset(s_pending_released, 0, sizeof(s_pending_released));
    memset(s_held, 0, sizeof(s_held));
    memset(s_down, 0, sizeof(s_down));
    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_released, 0, sizeof(s_released));
}

void raylib_lite_action_reset(void)
{
    raylib_lite_action_cancel();
    memset(s_zones, 0, sizeof(s_zones));
    s_zone_count = 0;
    s_axis_threshold = 250;
}

void raylib_lite_action_set_zones(const raylib_lite_action_zone_t *zones, size_t count)
{
    s_zone_count = 0;
    if (!zones) count = 0;
    if (count > RAYLIB_LITE_ACTION_ZONE_CAPACITY) count = RAYLIB_LITE_ACTION_ZONE_CAPACITY;
    for (size_t i = 0; i < count; ++i) {
        if (zones[i].action >= RAYLIB_LITE_ACTION_COUNT) continue;
        s_zones[s_zone_count++] = zones[i];
    }
    sync_zones();
}

void raylib_lite_action_set_axis_threshold(int32_t threshold)
{
    s_axis_threshold = threshold > 0 ? threshold : 250;
}

void raylib_lite_action_begin_frame(void)
{
    for (int i = 0; i < RAYLIB_LITE_ACTION_COUNT; ++i) {
        s_pressed[i] = s_pending_pressed[i];
        s_released[i] = s_pending_released[i];
        s_down[i] = s_held[i];
        s_pending_pressed[i] = false;
        s_pending_released[i] = false;
    }
}

void raylib_lite_action_apply_event(const raylib_lite_input_event_t *event)
{
    if (!event) return;
    if (event->type == RAYLIB_LITE_INPUT_DETACHED) {
        raylib_lite_action_cancel();
        return;
    }
    if (event->type == RAYLIB_LITE_INPUT_TOUCH ||
            event->type == RAYLIB_LITE_INPUT_POINTER) {
        int32_t track = event->type == RAYLIB_LITE_INPUT_TOUCH ? event->value : 0;
        raylib_lite_input_contact_t *contact = contact_for(event->type, track, event->pressed);
        if (contact) {
            contact->track_id = track;
            contact->x = event->x;
            contact->y = event->y;
            contact->active = event->pressed;
        }
        sync_zones();
    } else if (event->type == RAYLIB_LITE_INPUT_BUTTON) {
        int action = event->value;
        set_source(SOURCE_BUTTONS, action, event->pressed);
    } else if (event->type == RAYLIB_LITE_INPUT_JOYSTICK ||
               event->type == RAYLIB_LITE_INPUT_IMU) {
        enum action_source source = event->type == RAYLIB_LITE_INPUT_IMU
            ? SOURCE_IMU : SOURCE_JOYSTICK;
        set_source(source, RAYLIB_LITE_ACTION_LEFT, event->x < -s_axis_threshold);
        set_source(source, RAYLIB_LITE_ACTION_RIGHT, event->x > s_axis_threshold);
    }
}

bool raylib_lite_action_down(int action)
{
    return (unsigned)action < RAYLIB_LITE_ACTION_COUNT && s_down[action];
}

bool raylib_lite_action_pressed(int action)
{
    return (unsigned)action < RAYLIB_LITE_ACTION_COUNT && s_pressed[action];
}

bool raylib_lite_action_released(int action)
{
    return (unsigned)action < RAYLIB_LITE_ACTION_COUNT && s_released[action];
}

raylib_lite_input_contact_t raylib_lite_action_contact(size_t index)
{
    return index < RAYLIB_LITE_ACTION_CONTACT_CAPACITY ? s_contacts[index]
        : (raylib_lite_input_contact_t){0};
}

size_t raylib_lite_action_contact_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; ++i)
        if (s_contacts[i].active) ++count;
    return count;
}
