// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_action.h"
#include <string.h>

static mosaico_input_contact_t s_contacts[MOSAICO_CONTACT_CAPACITY];
static mosaico_action_zone_t s_zones[MOSAICO_ACTION_ZONE_CAPACITY];
static size_t s_zone_count;
static int32_t s_axis_threshold = 250;
static bool s_held[MOSAICO_ACTION_COUNT];
static bool s_down[MOSAICO_ACTION_COUNT];
static bool s_pressed[MOSAICO_ACTION_COUNT];
static bool s_released[MOSAICO_ACTION_COUNT];
static bool s_pulse[MOSAICO_ACTION_COUNT];

static void set_held(int action, bool down)
{
    if ((unsigned)action < MOSAICO_ACTION_COUNT) s_held[action] = down;
}

static void pulse(int action)
{
    if ((unsigned)action < MOSAICO_ACTION_COUNT) s_pulse[action] = true;
}

static void sync_zones(void)
{
    bool used[MOSAICO_ACTION_COUNT] = {0};
    bool down[MOSAICO_ACTION_COUNT] = {0};
    for (size_t z = 0; z < s_zone_count; ++z) {
        used[s_zones[z].action] = true;
        for (size_t i = 0; i < MOSAICO_CONTACT_CAPACITY; ++i) {
            if (!s_contacts[i].active) continue;
            if (s_contacts[i].x >= s_zones[z].x0 && s_contacts[i].x < s_zones[z].x1 &&
                s_contacts[i].y >= s_zones[z].y0 && s_contacts[i].y < s_zones[z].y1)
                down[s_zones[z].action] = true;
        }
    }
    for (int action = 0; action < MOSAICO_ACTION_COUNT; ++action)
        if (used[action]) set_held(action, down[action]);
}

static mosaico_input_contact_t *contact_for(int32_t track_id, bool pressed)
{
    for (size_t i = 0; i < MOSAICO_CONTACT_CAPACITY; ++i)
        if (s_contacts[i].active && s_contacts[i].track_id == track_id)
            return &s_contacts[i];
    if (!pressed) return NULL;
    for (size_t i = 0; i < MOSAICO_CONTACT_CAPACITY; ++i)
        if (!s_contacts[i].active) return &s_contacts[i];
    return NULL;
}

void mosaico_action_reset(void)
{
    memset(s_contacts, 0, sizeof(s_contacts));
    memset(s_zones, 0, sizeof(s_zones));
    memset(s_held, 0, sizeof(s_held));
    memset(s_down, 0, sizeof(s_down));
    memset(s_pressed, 0, sizeof(s_pressed));
    memset(s_released, 0, sizeof(s_released));
    memset(s_pulse, 0, sizeof(s_pulse));
    s_zone_count = 0;
    s_axis_threshold = 250;
}

void mosaico_action_set_zones(const mosaico_action_zone_t *zones, size_t count)
{
    if (!zones) { s_zone_count = 0; return; }
    if (count > MOSAICO_ACTION_ZONE_CAPACITY) count = MOSAICO_ACTION_ZONE_CAPACITY;
    memcpy(s_zones, zones, count * sizeof(*zones));
    s_zone_count = count;
}

void mosaico_action_set_axis_threshold(int32_t threshold)
{
    s_axis_threshold = threshold > 0 ? threshold : 250;
}

void mosaico_action_begin_frame(void)
{
    for (int i = 0; i < MOSAICO_ACTION_COUNT; ++i) {
        bool held = s_held[i] || s_pulse[i];
        s_pressed[i] = held && !s_down[i];
        s_released[i] = !held && s_down[i];
        s_down[i] = held;
        s_pulse[i] = false;
    }
}

void mosaico_action_apply_event(const mosaico_device_event_t *event)
{
    if (!event) return;
    if (event->type == MOSAICO_DEVICE_EVENT_TOUCH ||
            event->type == MOSAICO_DEVICE_EVENT_POINTER) {
        int32_t track = event->type == MOSAICO_DEVICE_EVENT_TOUCH ? event->value : 0;
        mosaico_input_contact_t *contact = contact_for(track, event->pressed);
        if (contact) {
            bool was_active = contact->active;
            contact->track_id = track;
            contact->x = event->x;
            contact->y = event->y;
            contact->active = event->pressed;
            if (event->pressed && !was_active) pulse(MOSAICO_ACTION_RESTART);
        }
        sync_zones();
    } else if (event->type == MOSAICO_DEVICE_EVENT_BUTTON) {
        int action = event->value;
        if (action < 0 || action >= MOSAICO_ACTION_COUNT) action = MOSAICO_ACTION_PAUSE;
        set_held(action, event->pressed);
        if (event->pressed) pulse(action);
    } else if (event->type == MOSAICO_DEVICE_EVENT_JOYSTICK ||
               event->type == MOSAICO_DEVICE_EVENT_IMU) {
        set_held(MOSAICO_ACTION_LEFT, event->x < -s_axis_threshold);
        set_held(MOSAICO_ACTION_RIGHT, event->x > s_axis_threshold);
    }
}

bool mosaico_action_down(int action)
{
    return (unsigned)action < MOSAICO_ACTION_COUNT && s_down[action];
}

bool mosaico_action_pressed(int action)
{
    return (unsigned)action < MOSAICO_ACTION_COUNT && s_pressed[action];
}

bool mosaico_action_released(int action)
{
    return (unsigned)action < MOSAICO_ACTION_COUNT && s_released[action];
}

mosaico_input_contact_t mosaico_action_contact(size_t index)
{
    return index < MOSAICO_CONTACT_CAPACITY ? s_contacts[index]
        : (mosaico_input_contact_t){0};
}

size_t mosaico_action_contact_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < MOSAICO_CONTACT_CAPACITY; ++i)
        if (s_contacts[i].active) ++count;
    return count;
}
