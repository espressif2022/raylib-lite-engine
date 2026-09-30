// SPDX-License-Identifier: Apache-2.0
#include "cat_rig.h"

#include <math.h>
#include <string.h>


static const float s_duration[CAT_ACT_COUNT] = {1.4f, 1.8f, 2.2f};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static float approach(float v, float target, float rate, float dt)
{
    return v + (target - v) * (1 - expf(-rate * dt));
}

static float rnd(cat_rig_t *rig)
{
    rig->seed = rig->seed * 1664525U + 1013904223U;
    return (float)(rig->seed >> 8) * (1.0f / 16777216.0f);
}

/* 0 -> 1 -> 0 envelope with eased in/out holds. */
static float envelope(float t, float duration, float attack, float release)
{
    if (t <= 0 || t >= duration) return 0;
    if (t < attack) { float u = t / attack; return u * u * (3 - 2 * u); }
    if (t > duration - release) { float u = (duration - t) / release; return u * u * (3 - 2 * u); }
    return 1;
}

const char *cat_action_name(int action)
{
    static const char *const names[] = {"slow blink", "tilt", "yawn"};
    return action >= 0 && action < CAT_ACT_COUNT ? names[action] : "?";
}

void cat_rig_init(cat_rig_t *rig, uint32_t seed)
{
    memset(rig, 0, sizeof(*rig));
    rig->seed = seed ? seed : 1;
    rig->pose.eye_open = 1;
    rig->blink_timer = 2;
    rig->blink_phase = -1;
    rig->twitch_timer = 2.5f;
    rig->idle_timer = 4;
    rig->tilt_dir = 1;
    for (int i = 0; i < CAT_ACT_COUNT; ++i) rig->action_time[i] = -1;
}

void cat_rig_play(cat_rig_t *rig, cat_action_t action)
{
    if ((unsigned)action >= CAT_ACT_COUNT) return;
    rig->action_time[action] = 0;
    if (action == CAT_ACT_TILT) rig->tilt_dir = -rig->tilt_dir;
    rig->idle_timer = 5 + rnd(rig) * 5;
}

bool cat_rig_playing(const cat_rig_t *rig, cat_action_t action)
{
    return (unsigned)action < CAT_ACT_COUNT && rig->action_time[action] >= 0;
}

void cat_rig_look(cat_rig_t *rig, float x, float y, bool active)
{
    rig->look_active = active;
    rig->look_tx = clampf((x - CAT_X) / 170, -1, 1);
    rig->look_ty = clampf((y - (CAT_GROUND_Y + CAT_HEAD_Y)) / 170, -1, 1);
}

void cat_rig_update(cat_rig_t *rig, float dt)
{
    cat_pose_t *pose = &rig->pose;
    rig->time += dt;
    pose->time = rig->time;

    float act[CAT_ACT_COUNT];
    for (int i = 0; i < CAT_ACT_COUNT; ++i) {
        if (rig->action_time[i] >= 0) {
            rig->action_time[i] += dt;
            if (rig->action_time[i] >= s_duration[i]) rig->action_time[i] = -1;
        }
        act[i] = rig->action_time[i] >= 0 ? rig->action_time[i] : -1;
    }

    rig->idle_timer -= dt;
    if (rig->idle_timer <= 0 && !rig->look_active) {
        float r = rnd(rig);
        cat_rig_play(rig, r < 0.45f ? CAT_ACT_BLINK : r < 0.8f ? CAT_ACT_TILT : CAT_ACT_YAWN);
    }

    float lx, ly;
    if (rig->look_active) {
        lx = rig->look_tx;
        ly = rig->look_ty;
    } else {
        rig->look_timer -= dt;
        if (rig->look_timer <= 0) {
            rig->look_timer = 1.5f + rnd(rig) * 3.5f;
            bool center = rnd(rig) < 0.45f;
            rig->wander_x = center ? 0 : (rnd(rig) * 2 - 1) * 0.7f;
            rig->wander_y = center ? 0 : (rnd(rig) * 2 - 1) * 0.35f;
        }
        lx = rig->wander_x;
        ly = rig->wander_y;
    }
    pose->look_x = approach(pose->look_x, lx, 6, dt);
    pose->look_y = approach(pose->look_y, ly, 6, dt);
    pose->pupil = approach(pose->pupil, rig->look_active ? 0.85f : 0.25f, 3, dt);

    float yawn = act[CAT_ACT_YAWN] >= 0 ? envelope(act[CAT_ACT_YAWN], 2.2f, 0.6f, 0.6f) : 0;
    float slow = act[CAT_ACT_BLINK] >= 0 ? envelope(act[CAT_ACT_BLINK], 1.4f, 0.45f, 0.5f) : 0;
    float tilt = act[CAT_ACT_TILT] >= 0 ? envelope(act[CAT_ACT_TILT], 1.8f, 0.35f, 0.5f) : 0;
    pose->mouth = yawn;

    float open = 1;
    rig->blink_timer -= dt;
    if (rig->blink_phase < 0 && rig->blink_timer <= 0) rig->blink_phase = 0;
    if (rig->blink_phase >= 0) {
        open = fabsf(1 - 2 * rig->blink_phase / 0.16f);
        rig->blink_phase += dt;
        if (rig->blink_phase >= 0.16f) {
            rig->blink_phase = -1;
            rig->blink_timer = 2 + rnd(rig) * 4;
        }
    }
    open *= 1 - 0.9f * slow;
    open *= 1 - 0.75f * yawn;
    pose->eye_open = open;

    float tilt_target = rig->tilt_dir * 0.22f * tilt + pose->look_x * 0.05f +
                        0.02f * sinf(rig->time * 0.6f);
    rig->tilt.velocity += (-70 * (rig->tilt.value - tilt_target) - 10 * rig->tilt.velocity) * dt;
    rig->tilt.value += rig->tilt.velocity * dt;
    pose->tilt = rig->tilt.value;

    rig->twitch_timer -= dt;
    if (rig->twitch_timer <= 0) {
        rig->twitch_timer = 2 + rnd(rig) * 5;
        rig->ear[rnd(rig) < 0.5f ? 0 : 1].velocity += 8;
    }
    for (int i = 0; i < 2; ++i) {
        cat_spring_t *s = &rig->ear[i];
        /* A curious tilt perks the upper ear; a yawn folds both back. */
        float side = i == 0 ? -1.0f : 1.0f;
        float target = -0.12f * tilt * side * rig->tilt_dir + 0.25f * yawn;
        s->velocity += (-160 * (s->value - target) - 10 * s->velocity) * dt;
        s->value += s->velocity * dt;
        pose->ear[i] = s->value;
    }
    pose->breath = sinf(rig->time * 2.0f) + yawn * 1.5f;
    pose->tail = 0.5f * sinf(rig->time * 1.3f) + 0.25f * sinf(rig->time * 3.1f);
}
