// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Screen-space layout shared by the simulation and the drawing code. */
#define PET_X 240.0f
#define PET_FLOOR_Y 372.0f
#define PET_HEAD_RISE 200.0f
#define PET_BALL_R 20.0f
#define PET_BALL_FLOOR 372.0f
#define PET_KIBBLE 6
#define PET_FX_COUNT 24

typedef enum {
    PET_MODE_IDLE,
    PET_MODE_EAT,
    PET_MODE_PLAY,
    PET_MODE_SLEEP,
} pet_mode_t;

typedef enum { PET_FX_HEART, PET_FX_Z } pet_fx_kind_t;

typedef enum {
    PET_TOUCH_NONE,
    PET_TOUCH_HEAD,
    PET_TOUCH_BODY,
    PET_TOUCH_BALL,
} pet_touch_t;

typedef struct {
    float x, y, vx, vy, age, life, size, phase;
    uint8_t kind;
    bool live;
} pet_fx_t;

typedef struct { float value, velocity; } pet_spring_t;

/* Everything the renderer needs; all of it is procedurally driven. */
typedef struct {
    float head_x, head_y, head_tilt;
    float look_x, look_y;
    float eye_open, eye_happy;
    float mouth_open, blush, sad;
    float breath;
    float ear_l, ear_r;
    float tail;
    float paw[2];
    float purr;
} pet_pose_t;

typedef struct {
    pet_pose_t pose;
    pet_mode_t mode;
    float time, mode_time, idle_time;
    float love, food, energy;

    bool pointer, moved, petting, ball_held;
    pet_touch_t touch;
    float px, py, down_x, down_y, stroke_travel, last_stroke;

    float blink_timer, blink_phase;
    float meow_timer, yawn_timer, happy_timer, surprise_timer;
    float look_timer, wander_x, wander_y, flick_timer, z_timer;
    pet_spring_t ear_l, ear_r, tail, tilt;

    int kibble;
    float chomp_timer;

    float ball_x, ball_y, ball_vx, ball_vy, ball_spin, swipe_cooldown;
    float paw_timer[2];

    pet_fx_t fx[PET_FX_COUNT];
    uint32_t seed;
} pet_t;

void pet_init(pet_t *pet, uint32_t seed);
void pet_update(pet_t *pet, float dt);
/* Selecting the current mode again returns to idle. */
void pet_toggle_mode(pet_t *pet, pet_mode_t mode);
void pet_pointer(pet_t *pet, bool pressed, float x, float y);
void pet_poke(pet_t *pet);
const char *pet_mode_name(pet_mode_t mode);
