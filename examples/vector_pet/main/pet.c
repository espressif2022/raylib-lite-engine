// SPDX-License-Identifier: Apache-2.0
#include "pet.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265f
#define MEOW_TIME 0.6f
#define YAWN_TIME 1.6f
#define SWIPE_TIME 0.38f
#define CHOMP_TIME 0.55f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static float approach(float v, float target, float rate, float dt)
{
    return v + (target - v) * (1 - expf(-rate * dt));
}

static float rnd(pet_t *pet)
{
    pet->seed = pet->seed * 1664525U + 1013904223U;
    return (float)(pet->seed >> 8) * (1.0f / 16777216.0f);
}

static void spring(pet_spring_t *s, float target, float k, float c, float dt)
{
    s->velocity += (-k * (s->value - target) - c * s->velocity) * dt;
    s->value += s->velocity * dt;
}

static void head_center(const pet_t *pet, float *x, float *y)
{
    *x = PET_X + pet->pose.head_x;
    *y = PET_FLOOR_Y - PET_HEAD_RISE + pet->pose.head_y;
}

static void spawn(pet_t *pet, pet_fx_kind_t kind, float x, float y)
{
    for (int i = 0; i < PET_FX_COUNT; ++i) {
        pet_fx_t *fx = &pet->fx[i];
        if (fx->live) continue;
        memset(fx, 0, sizeof(*fx));
        fx->live = true;
        fx->kind = (uint8_t)kind;
        fx->x = x;
        fx->y = y;
        fx->phase = rnd(pet) * 6.28f;
        if (kind == PET_FX_HEART) {
            fx->vx = (rnd(pet) - 0.5f) * 40;
            fx->vy = -70 - rnd(pet) * 30;
            fx->life = 1.2f;
            fx->size = 9 + rnd(pet) * 5;
        } else {
            fx->vx = 22;
            fx->vy = -28;
            fx->life = 2.2f;
            fx->size = 9;
        }
        return;
    }
}

void pet_init(pet_t *pet, uint32_t seed)
{
    memset(pet, 0, sizeof(*pet));
    pet->seed = seed ? seed : 1;
    pet->love = 70;
    pet->food = 60;
    pet->energy = 80;
    pet->blink_timer = 1.8f;
    pet->blink_phase = -1;
    pet->flick_timer = 3;
    pet->pose.eye_open = 1;
}

const char *pet_mode_name(pet_mode_t mode)
{
    static const char *const names[] = {"idle", "eat", "play", "sleep"};
    return (unsigned)mode < 4 ? names[mode] : "?";
}

static void set_mode(pet_t *pet, pet_mode_t mode)
{
    pet->mode = mode;
    pet->mode_time = 0;
    pet->idle_time = 0;
    pet->ball_held = false;
    if (mode == PET_MODE_EAT) {
        pet->kibble = PET_KIBBLE;
        pet->chomp_timer = CHOMP_TIME;
    } else if (mode == PET_MODE_PLAY) {
        pet->ball_x = PET_X + 160;
        pet->ball_y = 250;
        pet->ball_vx = -140;
        pet->ball_vy = 0;
        pet->swipe_cooldown = 0.6f;
        pet->surprise_timer = 0.5f;
    } else if (mode == PET_MODE_SLEEP) {
        pet->z_timer = 0.8f;
    }
}

void pet_toggle_mode(pet_t *pet, pet_mode_t mode)
{
    if (pet->mode == PET_MODE_SLEEP && mode != PET_MODE_SLEEP)
        pet->surprise_timer = 0.5f;
    set_mode(pet, pet->mode == mode ? PET_MODE_IDLE : mode);
}

void pet_poke(pet_t *pet)
{
    pet->idle_time = 0;
    if (pet->mode == PET_MODE_SLEEP) {
        set_mode(pet, PET_MODE_IDLE);
        pet->surprise_timer = 0.6f;
        pet->love = clampf(pet->love - 3, 0, 100);
        return;
    }
    pet->meow_timer = MEOW_TIME;
    pet->love = clampf(pet->love + 2, 0, 100);
    if (pet->love > 55) {
        float hx, hy;
        head_center(pet, &hx, &hy);
        spawn(pet, PET_FX_HEART, hx + 118, hy - 30);
    }
}

static bool in_ellipse(float x, float y, float cx, float cy, float rx, float ry)
{
    float dx = (x - cx) / rx, dy = (y - cy) / ry;
    return dx * dx + dy * dy <= 1;
}

static pet_touch_t hit(const pet_t *pet, float x, float y)
{
    if (pet->mode == PET_MODE_PLAY) {
        float dx = x - pet->ball_x, dy = y - pet->ball_y;
        if (dx * dx + dy * dy < 34 * 34) return PET_TOUCH_BALL;
    }
    float hx, hy;
    head_center(pet, &hx, &hy);
    if (in_ellipse(x, y, hx, hy - 10, 120, 110)) return PET_TOUCH_HEAD;
    if (in_ellipse(x, y, PET_X, PET_FLOOR_Y - 70, 90, 80)) return PET_TOUCH_BODY;
    return PET_TOUCH_NONE;
}

void pet_pointer(pet_t *pet, bool pressed, float x, float y)
{
    if (pressed && !pet->pointer) {
        pet->pointer = true;
        pet->moved = false;
        pet->petting = false;
        pet->down_x = x;
        pet->down_y = y;
        pet->touch = hit(pet, x, y);
        pet->ball_held = pet->touch == PET_TOUCH_BALL;
        pet->idle_time = 0;
    } else if (pressed) {
        float dx = x - pet->px, dy = y - pet->py;
        float ddx = x - pet->down_x, ddy = y - pet->down_y;
        if (ddx * ddx + ddy * ddy > 12 * 12) pet->moved = true;
        if (pet->moved && pet->touch != PET_TOUCH_BALL && pet->touch != PET_TOUCH_NONE &&
                hit(pet, x, y) == PET_TOUCH_HEAD) {
            pet->petting = true;
            pet->last_stroke = pet->time;
            pet->stroke_travel += sqrtf(dx * dx + dy * dy);
        }
        pet->idle_time = 0;
    } else if (pet->pointer) {
        pet->pointer = false;
        if (!pet->ball_held && !pet->moved &&
                (pet->touch == PET_TOUCH_HEAD || pet->touch == PET_TOUCH_BODY))
            pet_poke(pet);
        pet->ball_held = false;
        pet->petting = false;
    }
    pet->px = x;
    pet->py = y;
}

static void update_ball(pet_t *pet, float dt)
{
    if (pet->ball_held) {
        float tx = clampf(pet->px, 30, 450), ty = clampf(pet->py, 80, PET_BALL_FLOOR);
        pet->ball_vx = approach(pet->ball_vx, (tx - pet->ball_x) / dt, 20, dt);
        pet->ball_vy = approach(pet->ball_vy, (ty - pet->ball_y) / dt, 20, dt);
        pet->ball_spin += (tx - pet->ball_x) / PET_BALL_R;
        pet->ball_x = tx;
        pet->ball_y = ty;
        return;
    }
    pet->ball_vy += 1500 * dt;
    pet->ball_x += pet->ball_vx * dt;
    pet->ball_y += pet->ball_vy * dt;
    pet->ball_spin += pet->ball_vx * dt / PET_BALL_R;
    if (pet->ball_y > PET_BALL_FLOOR) {
        pet->ball_y = PET_BALL_FLOOR;
        pet->ball_vy = fabsf(pet->ball_vy) > 90 ? -pet->ball_vy * 0.55f : 0;
        pet->ball_vx *= powf(0.2f, dt);
    }
    if (pet->ball_x < 30) { pet->ball_x = 30; pet->ball_vx = fabsf(pet->ball_vx) * 0.7f; }
    if (pet->ball_x > 450) { pet->ball_x = 450; pet->ball_vx = -fabsf(pet->ball_vx) * 0.7f; }

    pet->swipe_cooldown -= dt;
    float dx = pet->ball_x - PET_X;
    if (pet->swipe_cooldown <= 0 && fabsf(dx) < 120 && pet->ball_y > PET_FLOOR_Y - 130) {
        int side = dx < 0 ? 0 : 1;
        float dir = dx < 0 ? -1.0f : 1.0f;
        if (fabsf(dx) < 8) dir = rnd(pet) < 0.5f ? -1.0f : 1.0f;
        pet->paw_timer[side] = SWIPE_TIME;
        pet->ball_vx = dir * (380 + rnd(pet) * 240);
        pet->ball_vy = -(360 + rnd(pet) * 300);
        pet->swipe_cooldown = 0.9f + rnd(pet) * 0.5f;
        pet->happy_timer = 0.5f;
        pet->love = clampf(pet->love + 1.5f, 0, 100);
        if (rnd(pet) < 0.35f) pet->meow_timer = MEOW_TIME;
    }
}

static void update_fx(pet_t *pet, float dt)
{
    for (int i = 0; i < PET_FX_COUNT; ++i) {
        pet_fx_t *fx = &pet->fx[i];
        if (!fx->live) continue;
        fx->age += dt;
        if (fx->age >= fx->life) { fx->live = false; continue; }
        fx->x += (fx->vx + sinf(fx->age * 5 + fx->phase) * 18) * dt;
        fx->y += fx->vy * dt;
        if (fx->kind == PET_FX_Z) fx->size += 5 * dt;
    }
}

void pet_update(pet_t *pet, float dt)
{
    pet_pose_t *pose = &pet->pose;
    pet->time += dt;
    pet->mode_time += dt;
    pet->idle_time += dt;
    const float t = pet->time;

    pet->food = clampf(pet->food - dt * 0.35f, 0, 100);
    pet->love = clampf(pet->love - dt * 0.2f, 0, 100);
    if (pet->mode == PET_MODE_SLEEP) pet->energy = clampf(pet->energy + dt * 6, 0, 100);
    else pet->energy = clampf(pet->energy - dt * (pet->mode == PET_MODE_PLAY ? 1.2f : 0.3f),
                              0, 100);

    pet->meow_timer -= dt;
    pet->yawn_timer -= dt;
    pet->happy_timer -= dt;
    pet->surprise_timer -= dt;
    for (int i = 0; i < 2; ++i) pet->paw_timer[i] -= dt;

    float hx, hy;
    head_center(pet, &hx, &hy);
    bool stroking = pet->petting && t - pet->last_stroke < 0.35f;
    while (pet->stroke_travel >= 70) {
        pet->stroke_travel -= 70;
        spawn(pet, PET_FX_HEART, pet->px, pet->py - 20);
        pet->love = clampf(pet->love + 2, 0, 100);
    }

    bool chomping = false;
    if (pet->mode == PET_MODE_EAT) {
        if (pet->mode_time > 0.5f) {
            pet->chomp_timer -= dt;
            if (pet->kibble > 0 && pet->chomp_timer <= 0) {
                pet->chomp_timer = CHOMP_TIME;
                --pet->kibble;
                pet->food = clampf(pet->food + 9, 0, 100);
                if (rnd(pet) < 0.5f) spawn(pet, PET_FX_HEART, hx + 118, hy - 20);
            }
            chomping = pet->kibble > 0 || pet->chomp_timer > 0;
            if (pet->kibble == 0 && pet->chomp_timer < -0.4f) {
                set_mode(pet, PET_MODE_IDLE);
                pet->happy_timer = 1.6f;
                pet->meow_timer = MEOW_TIME;
            }
        }
    } else if (pet->mode == PET_MODE_PLAY) {
        update_ball(pet, dt);
    } else if (pet->mode == PET_MODE_SLEEP) {
        pet->z_timer -= dt;
        if (pet->z_timer <= 0) {
            pet->z_timer = 1.1f;
            spawn(pet, PET_FX_Z, hx + 120, hy - 40);
        }
        if (pet->energy >= 100) {
            set_mode(pet, PET_MODE_IDLE);
            pet->yawn_timer = YAWN_TIME;
        }
    } else {
        if (pet->energy < 20 && pet->idle_time > 6) set_mode(pet, PET_MODE_SLEEP);
        else if (pet->energy < 50 && pet->yawn_timer < -9 && rnd(pet) < dt * 0.3f)
            pet->yawn_timer = YAWN_TIME;
    }

    /* Gaze */
    float lx, ly;
    if (pet->pointer) {
        lx = (pet->px - hx) / 170;
        ly = (pet->py - hy) / 170;
    } else if (pet->mode == PET_MODE_PLAY) {
        lx = (pet->ball_x - hx) / 170;
        ly = (pet->ball_y - hy) / 170;
    } else if (pet->mode == PET_MODE_EAT) {
        lx = 0;
        ly = 1;
    } else if (pet->mode == PET_MODE_SLEEP) {
        lx = 0;
        ly = 0.3f;
    } else {
        pet->look_timer -= dt;
        if (pet->look_timer <= 0) {
            pet->look_timer = 1.2f + rnd(pet) * 3;
            bool center = rnd(pet) < 0.35f;
            pet->wander_x = center ? 0 : (rnd(pet) * 2 - 1) * 0.8f;
            pet->wander_y = center ? 0 : (rnd(pet) * 2 - 1) * 0.5f;
        }
        lx = pet->wander_x;
        ly = pet->wander_y;
    }
    float look_rate = pet->mode == PET_MODE_PLAY ? 12.0f : 6.0f;
    pose->look_x = approach(pose->look_x, clampf(lx, -1, 1), look_rate, dt);
    pose->look_y = approach(pose->look_y, clampf(ly, -1, 1), look_rate, dt);

    /* Expression */
    bool happy = stroking || pet->happy_timer > 0 || chomping;
    bool sad = pet->mode == PET_MODE_IDLE && (pet->food < 25 || pet->love < 20);
    float yawn = pet->yawn_timer > 0 ? sinf(PI_F * (1 - pet->yawn_timer / YAWN_TIME)) : 0;
    float meow = pet->meow_timer > 0 ? sinf(PI_F * (1 - pet->meow_timer / MEOW_TIME)) : 0;
    float eye = 1;
    if (pet->mode == PET_MODE_SLEEP) eye = 0;
    else if (pet->surprise_timer > 0) eye = 1.25f;
    else if (pet->mode == PET_MODE_PLAY) eye = 1.1f;
    else if (sad) eye = 0.8f;
    eye *= 1 - yawn * 0.8f;
    pose->eye_happy = approach(pose->eye_happy, happy && pet->surprise_timer <= 0 ? 1.0f : 0.0f,
                               12, dt);
    float open = approach(pose->eye_open, eye, 14, dt);
    pet->blink_timer -= dt;
    if (pet->blink_phase < 0 && pet->blink_timer <= 0) pet->blink_phase = 0;
    float blink = 1;
    if (pet->blink_phase >= 0) {
        blink = fabsf(1 - 2 * pet->blink_phase / 0.15f);
        pet->blink_phase += dt;
        if (pet->blink_phase >= 0.15f) {
            pet->blink_phase = -1;
            pet->blink_timer = 1.8f + rnd(pet) * 3.5f;
        }
    }
    pose->eye_open = open;
    if (blink < 1) pose->eye_open = open * blink;

    float mouth = meow * 0.8f;
    if (yawn > mouth) mouth = yawn;
    if (chomping) mouth = 0.18f + 0.22f * (0.5f + 0.5f * sinf(t * 16));
    pose->mouth_open = approach(pose->mouth_open, mouth, 18, dt);
    pose->blush = approach(pose->blush, happy ? 1.0f : pet->love > 70 ? 0.6f : 0.3f, 4, dt);
    pose->sad = approach(pose->sad, sad ? 1.0f : 0.0f, 3, dt);
    pose->purr = approach(pose->purr, stroking ? 1.0f : 0.0f, 6, dt);

    /* Body */
    pose->breath = pet->mode == PET_MODE_SLEEP ? sinf(t * 1.3f) * 1.6f : sinf(t * 2.4f) * 0.6f;
    float head_y = pose->breath * 1.2f;
    if (pet->mode == PET_MODE_SLEEP) head_y += 18;
    else if (pet->mode == PET_MODE_EAT) head_y += 44 + (chomping ? 4 * sinf(t * 16) : 0);
    head_y -= meow * 10 + (pet->surprise_timer > 0 ? 8 : 0);
    pose->head_y = approach(pose->head_y, head_y, 10, dt);
    pose->head_x = pose->look_x * 10;

    float tilt = pose->look_x * 0.1f + 0.04f * sinf(t * 0.5f);
    if (stroking) tilt = clampf((pet->px - hx) / 300, -0.25f, 0.25f);
    else if (pet->mode == PET_MODE_SLEEP) tilt = 0.14f;
    else if (pet->surprise_timer > 0) tilt = 0;
    spring(&pet->tilt, tilt, 60, 9, dt);
    pose->head_tilt = pet->tilt.value;

    float ear = 0;
    if (stroking) ear = 0.35f;
    else if (pet->mode == PET_MODE_SLEEP) ear = 0.25f;
    else if (sad) ear = 0.45f;
    else if (meow > 0 || pet->surprise_timer > 0) ear = -0.15f;
    pet->flick_timer -= dt;
    if (pet->flick_timer <= 0) {
        pet->flick_timer = 3 + rnd(pet) * 4;
        if (pet->mode != PET_MODE_SLEEP) {
            if (rnd(pet) < 0.5f) pet->ear_l.velocity += 7;
            else pet->ear_r.velocity += 7;
        }
    }
    spring(&pet->ear_l, ear, 140, 9, dt);
    spring(&pet->ear_r, ear, 140, 9, dt);
    pose->ear_l = pet->ear_l.value;
    pose->ear_r = pet->ear_r.value;

    float tail = 0.25f * sinf(t * 1.1f);
    if (pet->mode == PET_MODE_SLEEP) tail = 1.05f + 0.05f * sinf(t * 0.8f);
    else if (happy) tail = 0.45f * sinf(t * 6);
    else if (pet->mode == PET_MODE_PLAY) tail = 0.5f * sinf(t * 3.2f);
    else if (sad) tail = 0.55f;
    spring(&pet->tail, tail, 30, 5, dt);
    pose->tail = pet->tail.value;

    for (int i = 0; i < 2; ++i)
        pose->paw[i] = pet->paw_timer[i] > 0 ? sinf(PI_F * (1 - pet->paw_timer[i] / SWIPE_TIME))
                                             : 0;
    update_fx(pet, dt);
}
