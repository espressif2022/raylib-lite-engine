// SPDX-License-Identifier: Apache-2.0
#include "puppet_rig.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

typedef struct { float t, v; } puppet_key_t;
typedef struct {
    uint8_t param;
    uint8_t count;
    const puppet_key_t *keys;
} puppet_track_t;
typedef struct {
    const char *name;
    float duration;
    const puppet_track_t *tracks;
    uint8_t track_count;
} puppet_action_def_t;

#define KEYS(...) ((const puppet_key_t[]){__VA_ARGS__})
#define TRACK(param, ...) \
    {param, (uint8_t)(sizeof(KEYS(__VA_ARGS__)) / sizeof(puppet_key_t)), KEYS(__VA_ARGS__)}
#define ACTION(name, duration, tracks) \
    {name, duration, tracks, (uint8_t)(sizeof(tracks) / sizeof(tracks[0]))}

static const puppet_track_t s_happy[] = {
    TRACK(PUPPET_EYE_SMILE, {0, 0}, {0.15f, 1}, {1.35f, 1}, {1.6f, 0}),
    TRACK(PUPPET_MOUTH_OPEN, {0, 0}, {0.15f, 0.7f}, {1.3f, 0.6f}, {1.6f, 0}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.15f, 1}, {1.6f, 0.4f}),
    TRACK(PUPPET_BLUSH, {0, 0}, {0.3f, 0.6f}, {1.6f, 0.2f}),
    TRACK(PUPPET_BODY_BOUNCE, {0, 0}, {0.18f, -16}, {0.36f, 0}, {0.56f, -10},
          {0.74f, 0}),
    TRACK(PUPPET_HEAD_ROLL, {0, 0}, {0.25f, 7}, {0.7f, -5}, {1.2f, 3}, {1.6f, 0}),
    TRACK(PUPPET_HAND_L_X, {0, 0}, {0.2f, -18}, {1.3f, -18}, {1.6f, 0}),
    TRACK(PUPPET_HAND_L_Y, {0, 0}, {0.2f, -190}, {0.4f, -175}, {0.6f, -192},
          {1.3f, -185}, {1.6f, 0}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.2f, 18}, {1.3f, 18}, {1.6f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.2f, -190}, {0.4f, -175}, {0.6f, -192},
          {1.3f, -185}, {1.6f, 0}),
};
static const puppet_track_t s_wink[] = {
    TRACK(PUPPET_EYE_OPEN_R, {0, 1}, {0.12f, 0}, {0.85f, 0}, {1.0f, 1}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.15f, 0.9f}, {1.2f, 0.3f}),
    TRACK(PUPPET_HEAD_ROLL, {0, 0}, {0.2f, -9}, {0.9f, -8}, {1.2f, 0}),
    TRACK(PUPPET_BROW, {0, 0}, {0.2f, 0.4f}, {1.2f, 0}),
    TRACK(PUPPET_BODY_LEAN, {0, 0}, {0.25f, 0.35f}, {1.2f, 0}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.2f, -14}, {0.95f, -14}, {1.2f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.2f, -236}, {0.95f, -232}, {1.2f, 0}),
    TRACK(PUPPET_HAND_R_GESTURE, {0, PUPPET_HAND_PEACE}, {1.2f, PUPPET_HAND_PEACE}),
};
static const puppet_track_t s_nod[] = {
    TRACK(PUPPET_HEAD_PITCH, {0, 0}, {0.2f, 1}, {0.4f, -0.2f}, {0.62f, 0.9f},
          {0.85f, 0}),
    TRACK(PUPPET_EYE_OPEN_L, {0, 1}, {0.2f, 0.75f}, {0.9f, 1}),
    TRACK(PUPPET_EYE_OPEN_R, {0, 1}, {0.2f, 0.75f}, {0.9f, 1}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.2f, 0.6f}, {1.2f, 0.2f}),
};
static const puppet_track_t s_think[] = {
    TRACK(PUPPET_EYE_X, {0, 0}, {0.3f, 0.8f}, {2.0f, 0.7f}, {2.4f, 0}),
    TRACK(PUPPET_EYE_Y, {0, 0}, {0.3f, -0.9f}, {2.0f, -0.8f}, {2.4f, 0}),
    TRACK(PUPPET_HEAD_ROLL, {0, 0}, {0.4f, 11}, {2.0f, 10}, {2.4f, 0}),
    TRACK(PUPPET_HEAD_YAW, {0, 0}, {0.4f, 0.35f}, {2.0f, 0.3f}, {2.4f, 0}),
    TRACK(PUPPET_BROW, {0, 0}, {0.3f, -0.6f}, {2.0f, -0.5f}, {2.4f, 0}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.3f, -0.4f}, {2.4f, 0}),
    TRACK(PUPPET_MOUTH_OPEN, {0, 0}, {0.5f, 0.12f}, {2.0f, 0.1f}, {2.4f, 0}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.4f, -50}, {2.0f, -50}, {2.4f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.4f, -188}, {2.0f, -186}, {2.4f, 0}),
    TRACK(PUPPET_HAND_R_GESTURE, {0, PUPPET_HAND_POINT}, {2.4f, PUPPET_HAND_POINT}),
    TRACK(PUPPET_HAND_L_X, {0, 0}, {0.4f, 92}, {2.0f, 92}, {2.4f, 0}),
    TRACK(PUPPET_HAND_L_Y, {0, 0}, {0.4f, -62}, {2.0f, -62}, {2.4f, 0}),
};
static const puppet_track_t s_surprise[] = {
    TRACK(PUPPET_EYE_OPEN_L, {0, 1}, {0.1f, 1.3f}, {1.0f, 1.25f}, {1.4f, 1}),
    TRACK(PUPPET_EYE_OPEN_R, {0, 1}, {0.1f, 1.3f}, {1.0f, 1.25f}, {1.4f, 1}),
    TRACK(PUPPET_BROW, {0, 0}, {0.1f, 1}, {1.1f, 0.9f}, {1.4f, 0}),
    TRACK(PUPPET_MOUTH_OPEN, {0, 0}, {0.12f, 0.9f}, {1.0f, 0.8f}, {1.4f, 0}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.12f, -0.3f}, {1.4f, 0}),
    TRACK(PUPPET_BODY_BOUNCE, {0, 0}, {0.1f, -12}, {0.3f, 0}),
    TRACK(PUPPET_HEAD_PITCH, {0, 0}, {0.12f, -0.5f}, {1.0f, -0.4f}, {1.4f, 0}),
    TRACK(PUPPET_HAND_L_X, {0, 0}, {0.12f, 24}, {1.1f, 24}, {1.4f, 0}),
    TRACK(PUPPET_HAND_L_Y, {0, 0}, {0.12f, -196}, {1.1f, -192}, {1.4f, 0}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.12f, -24}, {1.1f, -24}, {1.4f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.12f, -196}, {1.1f, -192}, {1.4f, 0}),
};
static const puppet_track_t s_shy[] = {
    TRACK(PUPPET_BLUSH, {0, 0}, {0.4f, 1}, {2.0f, 1}, {2.4f, 0.1f}),
    TRACK(PUPPET_EYE_Y, {0, 0}, {0.35f, 0.9f}, {2.0f, 0.8f}, {2.4f, 0}),
    TRACK(PUPPET_EYE_X, {0, 0}, {0.35f, -0.6f}, {2.0f, -0.5f}, {2.4f, 0}),
    TRACK(PUPPET_EYE_OPEN_L, {0, 1}, {0.35f, 0.7f}, {2.0f, 0.7f}, {2.4f, 1}),
    TRACK(PUPPET_EYE_OPEN_R, {0, 1}, {0.35f, 0.7f}, {2.0f, 0.7f}, {2.4f, 1}),
    TRACK(PUPPET_HEAD_ROLL, {0, 0}, {0.45f, -12}, {2.0f, -11}, {2.4f, 0}),
    TRACK(PUPPET_HEAD_PITCH, {0, 0}, {0.45f, 0.5f}, {2.0f, 0.45f}, {2.4f, 0}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.4f, 0.45f}, {2.4f, 0}),
    TRACK(PUPPET_BODY_LEAN, {0, 0}, {0.5f, -0.35f}, {2.0f, -0.3f}, {2.4f, 0}),
    TRACK(PUPPET_HAND_L_X, {0, 0}, {0.45f, 66}, {2.0f, 66}, {2.4f, 0}),
    TRACK(PUPPET_HAND_L_Y, {0, 0}, {0.45f, -42}, {2.0f, -40}, {2.4f, 0}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.45f, -66}, {2.0f, -66}, {2.4f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.45f, -42}, {2.0f, -40}, {2.4f, 0}),
    TRACK(PUPPET_HAND_L_GESTURE, {0, PUPPET_HAND_FIST}, {2.4f, PUPPET_HAND_FIST}),
    TRACK(PUPPET_HAND_R_GESTURE, {0, PUPPET_HAND_FIST}, {2.4f, PUPPET_HAND_FIST}),
};
static const puppet_track_t s_shake[] = {
    TRACK(PUPPET_HEAD_YAW, {0, 0}, {0.15f, -0.8f}, {0.4f, 0.8f}, {0.65f, -0.7f},
          {0.9f, 0.5f}, {1.15f, 0}),
    TRACK(PUPPET_BROW, {0, 0}, {0.15f, -0.5f}, {1.4f, 0}),
    TRACK(PUPPET_MOUTH_SMILE, {0, 0}, {0.15f, -0.6f}, {1.4f, 0}),
    TRACK(PUPPET_EYE_OPEN_L, {0, 1}, {0.15f, 0.85f}, {1.4f, 1}),
    TRACK(PUPPET_EYE_OPEN_R, {0, 1}, {0.15f, 0.85f}, {1.4f, 1}),
    TRACK(PUPPET_HAND_R_X, {0, 0}, {0.25f, 22}, {1.15f, 22}, {1.4f, 0}),
    TRACK(PUPPET_HAND_R_Y, {0, 0}, {0.25f, -178}, {1.15f, -178}, {1.4f, 0}),
    TRACK(PUPPET_WAVE, {0, 0}, {0.3f, 1}, {1.1f, 1}, {1.4f, 0}),
};

static const puppet_action_def_t s_actions[PUPPET_ACTION_COUNT] = {
    [PUPPET_ACTION_IDLE] = {"idle", 0, NULL, 0},
    [PUPPET_ACTION_HAPPY] = ACTION("happy", 1.6f, s_happy),
    [PUPPET_ACTION_WINK] = ACTION("wink", 1.2f, s_wink),
    [PUPPET_ACTION_NOD] = ACTION("nod", 1.2f, s_nod),
    [PUPPET_ACTION_THINK] = ACTION("thinking", 2.4f, s_think),
    [PUPPET_ACTION_SURPRISE] = ACTION("surprised", 1.4f, s_surprise),
    [PUPPET_ACTION_SHY] = ACTION("shy", 2.4f, s_shy),
    [PUPPET_ACTION_SHAKE] = ACTION("shake head", 1.4f, s_shake),
};

static float rnd(puppet_rig_t *rig)
{
    rig->seed = rig->seed * 1664525U + 1013904223U;
    return (float)(rig->seed >> 8) * (1.0f / 16777216.0f);
}

static float smooth(float t) { return t * t * (3 - 2 * t); }

static float sample(const puppet_track_t *track, float t)
{
    const puppet_key_t *k = track->keys;
    if (t <= k[0].t) return k[0].v;
    for (int i = 1; i < track->count; ++i) {
        if (t <= k[i].t) {
            float span = k[i].t - k[i - 1].t;
            float u = span > 0 ? (t - k[i - 1].t) / span : 1;
            return k[i - 1].v + (k[i].v - k[i - 1].v) * smooth(u);
        }
    }
    return k[track->count - 1].v;
}

static void spring_step(puppet_spring_t *s, float target, float k, float c,
                        float impulse, float dt)
{
    float acc = -k * (s->angle - target) - c * s->velocity - impulse;
    s->velocity += acc * dt;
    s->angle += s->velocity * dt;
    if (s->angle > 40) { s->angle = 40; s->velocity = 0; }
    if (s->angle < -40) { s->angle = -40; s->velocity = 0; }
}

static const float s_limb_rest[PUPPET_LIMB_COUNT][2] = {
    {-76, 150}, {76, 150}, {-28, 372}, {28, 372},
};

void puppet_rig_init(puppet_rig_t *rig, uint32_t seed)
{
    memset(rig, 0, sizeof(*rig));
    rig->seed = seed ? seed : 1;
    rig->blink_timer = 1.5f;
    rig->blink_phase = -1;
    for (int i = 0; i < PUPPET_LIMB_COUNT; ++i) {
        rig->limbs[i].x = s_limb_rest[i][0];
        rig->limbs[i].y = s_limb_rest[i][1];
    }
    rig->gesture_override[0] = rig->gesture_override[1] = -1;
}

void puppet_rig_drag_limb(puppet_rig_t *rig, int limb, float x, float y)
{
    if (limb < 0 || limb >= PUPPET_LIMB_COUNT) return;
    puppet_limb_state_t *l = &rig->limbs[limb];
    l->dragging = true;
    l->drag_x = x;
    l->drag_y = y;
}

void puppet_rig_release_limb(puppet_rig_t *rig, int limb)
{
    if (limb < 0 || limb >= PUPPET_LIMB_COUNT) return;
    rig->limbs[limb].dragging = false;
}

void puppet_rig_cycle_gesture(puppet_rig_t *rig, int hand)
{
    if (hand < 0 || hand > 1) return;
    int current = rig->gesture_override[hand] >= 0 ? rig->gesture_override[hand]
                                                   : rig->pose.gesture[hand];
    rig->gesture_override[hand] = (current + 1) % PUPPET_HAND_GESTURE_COUNT;
}

static void update_limbs(puppet_rig_t *rig, float dt)
{
    const float *p = rig->pose.p;
    float wave = p[PUPPET_WAVE] * sinf(rig->time * 12) * 22;
    float goal[PUPPET_LIMB_COUNT][2] = {
        {s_limb_rest[0][0] + p[PUPPET_HAND_L_X] - 2 * sinf(rig->time * 0.9f),
         s_limb_rest[0][1] + p[PUPPET_HAND_L_Y] - rig->pose.breath * 2},
        {s_limb_rest[1][0] + p[PUPPET_HAND_R_X] + 2 * sinf(rig->time * 0.9f) + wave,
         s_limb_rest[1][1] + p[PUPPET_HAND_R_Y] - rig->pose.breath * 2 - fabsf(wave) * 0.3f},
        /* Feet stay planted while the body bounces. */
        {s_limb_rest[2][0], s_limb_rest[2][1] - p[PUPPET_BODY_BOUNCE]},
        {s_limb_rest[3][0], s_limb_rest[3][1] - p[PUPPET_BODY_BOUNCE]},
    };
    for (int i = 0; i < PUPPET_LIMB_COUNT; ++i) {
        puppet_limb_state_t *l = &rig->limbs[i];
        if (l->dragging) {
            l->x = l->drag_x;
            l->y = l->drag_y;
            l->vx = l->vy = 0;
        } else {
            const float k = 140, c = 15;
            l->vx += (-k * (l->x - goal[i][0]) - c * l->vx) * dt;
            l->vy += (-k * (l->y - goal[i][1]) - c * l->vy) * dt;
            l->x += l->vx * dt;
            l->y += l->vy * dt;
        }
        rig->pose.limb_x[i] = l->x;
        rig->pose.limb_y[i] = l->y;
    }
    for (int hand = 0; hand < 2; ++hand) {
        int g = rig->gesture_override[hand];
        if (g < 0) g = (int)lrintf(p[PUPPET_HAND_L_GESTURE + hand]);
        if (g < 0) g = 0;
        if (g >= PUPPET_HAND_GESTURE_COUNT) g = PUPPET_HAND_GESTURE_COUNT - 1;
        rig->pose.gesture[hand] = (uint8_t)g;
    }
}

void puppet_rig_play(puppet_rig_t *rig, int action)
{
    if (action < 0 || action >= PUPPET_ACTION_COUNT) action = PUPPET_ACTION_IDLE;
    rig->action = action;
    rig->action_time = 0;
}

void puppet_rig_set_look(puppet_rig_t *rig, float x, float y, bool active)
{
    rig->look_active = active;
    rig->look_x = x < -1 ? -1 : x > 1 ? 1 : x;
    rig->look_y = y < -1 ? -1 : y > 1 ? 1 : y;
}

void puppet_rig_set_tilt(puppet_rig_t *rig, float x)
{
    rig->tilt_x = x < -1 ? -1 : x > 1 ? 1 : x;
}

bool puppet_rig_action_done(const puppet_rig_t *rig)
{
    return rig->action == PUPPET_ACTION_IDLE ||
           rig->action_time >= s_actions[rig->action].duration;
}

const char *puppet_action_name(int action)
{
    if (action < 0 || action >= PUPPET_ACTION_COUNT) return "?";
    return s_actions[action].name;
}

void puppet_rig_update(puppet_rig_t *rig, float dt)
{
    rig->time += dt;
    rig->action_time += dt;
    float t = rig->time;
    float *p = rig->pose.p;

    /* Base layer: neutral pose plus idle sway. */
    for (int i = 0; i < PUPPET_PARAM_COUNT; ++i) p[i] = 0;
    p[PUPPET_EYE_OPEN_L] = p[PUPPET_EYE_OPEN_R] = 1;
    p[PUPPET_HEAD_ROLL] = 2.5f * sinf(t * 0.7f);
    p[PUPPET_HEAD_YAW] = 0.12f * sinf(t * 0.43f + 1.0f);
    p[PUPPET_BODY_LEAN] = 0.08f * sinf(t * 0.37f);
    p[PUPPET_MOUTH_SMILE] = 0.15f;
    rig->pose.breath = 0.5f + 0.5f * sinf(t * 1.8f);

    /* Action layer: keyed parameters override the base with a fade. */
    const puppet_action_def_t *def = &s_actions[rig->action];
    if (def->track_count && rig->action_time < def->duration) {
        float at = rig->action_time;
        float w = at < 0.18f ? smooth(at / 0.18f) : 1;
        float tail = def->duration - at;
        if (tail < 0.3f) w *= smooth(tail / 0.3f);
        for (int i = 0; i < def->track_count; ++i) {
            const puppet_track_t *track = &def->tracks[i];
            float v = sample(track, at);
            p[track->param] += (v - p[track->param]) * w;
        }
    }

    /* Procedural layers: look-at, tilt, blink. */
    float tx = rig->look_active ? rig->look_x : 0;
    float ty = rig->look_active ? rig->look_y : 0;
    float ease = 1 - expf(-dt * 8);
    rig->look_sx += (tx - rig->look_sx) * ease;
    rig->look_sy += (ty - rig->look_sy) * ease;
    p[PUPPET_EYE_X] += rig->look_sx * 0.9f;
    p[PUPPET_EYE_Y] += rig->look_sy * 0.8f;
    p[PUPPET_HEAD_YAW] += rig->look_sx * 0.35f;
    p[PUPPET_HEAD_PITCH] += rig->look_sy * 0.25f;
    p[PUPPET_HEAD_ROLL] += rig->tilt_x * 10;
    if (p[PUPPET_EYE_X] > 1) p[PUPPET_EYE_X] = 1;
    if (p[PUPPET_EYE_X] < -1) p[PUPPET_EYE_X] = -1;
    if (p[PUPPET_EYE_Y] > 1) p[PUPPET_EYE_Y] = 1;
    if (p[PUPPET_EYE_Y] < -1) p[PUPPET_EYE_Y] = -1;

    rig->blink_timer -= dt;
    if (rig->blink_phase < 0 && rig->blink_timer <= 0) rig->blink_phase = 0;
    if (rig->blink_phase >= 0) {
        const float duration = 0.16f;
        float b = fabsf(1 - 2 * rig->blink_phase / duration);
        p[PUPPET_EYE_OPEN_L] *= b;
        p[PUPPET_EYE_OPEN_R] *= b;
        rig->blink_phase += dt;
        if (rig->blink_phase >= duration) {
            rig->blink_phase = -1;
            rig->blink_timer = 2.0f + rnd(rig) * 3.5f;
        }
    }

    /* Physics layer: head motion drives hair springs. */
    float roll = p[PUPPET_HEAD_ROLL];
    float driver = p[PUPPET_HEAD_YAW] * 14 + roll * 0.9f + p[PUPPET_BODY_LEAN] * 22 +
                   p[PUPPET_BODY_BOUNCE] * 0.6f;
    float dv = 0;
    if (rig->driver_valid && dt > 0) dv = (driver - rig->driver_prev) / dt;
    rig->driver_prev = driver;
    rig->driver_valid = true;
    spring_step(&rig->tail_l, -roll * 0.85f, 55, 5.5f, dv * 0.8f, dt);
    spring_step(&rig->tail_r, -roll * 0.85f, 50, 5.0f, dv * 0.8f, dt);
    spring_step(&rig->ahoge, 0, 170, 7.0f, dv * 1.6f, dt);
    spring_step(&rig->sway, -roll * 0.4f, 38, 5.0f, dv * 0.45f, dt);
    rig->pose.tail_l = rig->tail_l.angle;
    rig->pose.tail_r = rig->tail_r.angle;
    rig->pose.ahoge = rig->ahoge.angle;
    rig->pose.hair_sway = rig->sway.angle;
    update_limbs(rig, dt);
}
