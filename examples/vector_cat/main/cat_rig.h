// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Front-facing sitting cat. Cat space: x right, y down, ground at y = 0. */

#define CAT_X 240.0f
#define CAT_GROUND_Y 404.0f
#define CAT_HEAD_Y -236.0f

typedef enum {
    CAT_ACT_BLINK,
    CAT_ACT_TILT,
    CAT_ACT_YAWN,
    CAT_ACT_COUNT
} cat_action_t;

typedef struct {
    float look_x, look_y;
    float eye_open;
    float pupil;        /* 0 slit .. 1 round */
    float ear[2];       /* left, right twitch angles */
    float breath;
    float tilt;         /* head roll, radians */
    float mouth;        /* 0 closed .. 1 yawn */
    float tail;
    float time;
} cat_pose_t;

typedef struct { float value, velocity; } cat_spring_t;

typedef struct {
    cat_pose_t pose;
    float time;
    bool look_active;
    float look_tx, look_ty;
    float look_timer, wander_x, wander_y;
    float blink_timer, blink_phase;
    bool slow_blink;
    float twitch_timer, idle_timer;
    float action_time[CAT_ACT_COUNT];
    float tilt_dir;
    cat_spring_t ear[2], tilt;
    uint32_t seed;
} cat_rig_t;

void cat_rig_init(cat_rig_t *rig, uint32_t seed);
void cat_rig_update(cat_rig_t *rig, float dt);
void cat_rig_play(cat_rig_t *rig, cat_action_t action);
bool cat_rig_playing(const cat_rig_t *rig, cat_action_t action);
/* Screen-space gaze target, or inactive to let the cat look around. */
void cat_rig_look(cat_rig_t *rig, float x, float y, bool active);
const char *cat_action_name(int action);
