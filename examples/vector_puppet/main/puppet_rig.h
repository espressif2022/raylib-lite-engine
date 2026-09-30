// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Named parameters are the only interface between motion and drawing. */
typedef enum {
    PUPPET_HEAD_YAW,     /* -1 left .. 1 right */
    PUPPET_HEAD_PITCH,   /* -1 up .. 1 down */
    PUPPET_HEAD_ROLL,    /* degrees */
    PUPPET_BODY_LEAN,    /* -1 .. 1 */
    PUPPET_BODY_BOUNCE,  /* pixels, negative is up */
    PUPPET_EYE_OPEN_L,   /* 0 closed .. 1 open .. 1.3 wide */
    PUPPET_EYE_OPEN_R,
    PUPPET_EYE_SMILE,    /* 0 .. 1 closed happy arcs */
    PUPPET_EYE_X,        /* -1 .. 1 */
    PUPPET_EYE_Y,        /* -1 up .. 1 down */
    PUPPET_BROW,         /* -1 frown .. 1 raised */
    PUPPET_MOUTH_OPEN,   /* 0 .. 1 */
    PUPPET_MOUTH_SMILE,  /* -1 .. 1 */
    PUPPET_BLUSH,        /* 0 .. 1 */
    PUPPET_HAND_L_X,     /* body-space offsets from the resting hand */
    PUPPET_HAND_L_Y,
    PUPPET_HAND_R_X,
    PUPPET_HAND_R_Y,
    PUPPET_HAND_L_GESTURE, /* puppet_hand_t, rounded */
    PUPPET_HAND_R_GESTURE,
    PUPPET_WAVE,         /* 0 .. 1 right-hand wave amplitude */
    PUPPET_PARAM_COUNT
} puppet_param_t;

/* L is the screen-left side. */
typedef enum {
    PUPPET_LIMB_ARM_L,
    PUPPET_LIMB_ARM_R,
    PUPPET_LIMB_LEG_L,
    PUPPET_LIMB_LEG_R,
    PUPPET_LIMB_COUNT
} puppet_limb_t;

typedef enum {
    PUPPET_HAND_OPEN,
    PUPPET_HAND_FIST,
    PUPPET_HAND_PEACE,
    PUPPET_HAND_POINT,
    PUPPET_HAND_GESTURE_COUNT
} puppet_hand_t;

/* Body space: origin at the neck base, y down. */
#define PUPPET_SHOULDER_X 58.0f
#define PUPPET_SHOULDER_Y 30.0f
#define PUPPET_UPPER_ARM 66.0f
#define PUPPET_FOREARM 60.0f
#define PUPPET_HIP_X 22.0f
#define PUPPET_HIP_Y 205.0f
#define PUPPET_THIGH 86.0f
#define PUPPET_SHIN 84.0f

typedef enum {
    PUPPET_ACTION_IDLE,
    PUPPET_ACTION_HAPPY,
    PUPPET_ACTION_WINK,
    PUPPET_ACTION_NOD,
    PUPPET_ACTION_THINK,
    PUPPET_ACTION_SURPRISE,
    PUPPET_ACTION_SHY,
    PUPPET_ACTION_SHAKE,
    PUPPET_ACTION_COUNT
} puppet_action_t;

typedef struct {
    float angle, velocity;
} puppet_spring_t;

typedef struct {
    float p[PUPPET_PARAM_COUNT];
    float breath;         /* 0 .. 1 */
    float tail_l, tail_r; /* degrees from spring chains */
    float ahoge;          /* degrees */
    float hair_sway;      /* degrees, back hair and side locks */
    float limb_x[PUPPET_LIMB_COUNT], limb_y[PUPPET_LIMB_COUNT]; /* hand/foot targets */
    uint8_t gesture[2];
} puppet_pose_t;

typedef struct {
    float x, y, vx, vy;
    bool dragging;
    float drag_x, drag_y;
} puppet_limb_state_t;

typedef struct {
    float time;
    int action;
    float action_time;
    float blink_timer, blink_phase;
    uint32_t seed;
    bool look_active;
    float look_x, look_y, look_sx, look_sy;
    float tilt_x;
    float driver_prev;
    bool driver_valid;
    puppet_spring_t tail_l, tail_r, ahoge, sway;
    puppet_limb_state_t limbs[PUPPET_LIMB_COUNT];
    int gesture_override[2];
    puppet_pose_t pose;
} puppet_rig_t;

void puppet_rig_init(puppet_rig_t *rig, uint32_t seed);
void puppet_rig_play(puppet_rig_t *rig, int action);
/* Normalized -1..1 look target; inactive eases back to centre. */
void puppet_rig_set_look(puppet_rig_t *rig, float x, float y, bool active);
void puppet_rig_set_tilt(puppet_rig_t *rig, float x);
void puppet_rig_update(puppet_rig_t *rig, float dt);
bool puppet_rig_action_done(const puppet_rig_t *rig);
/* Body-space target; the limb follows it exactly until released. */
void puppet_rig_drag_limb(puppet_rig_t *rig, int limb, float x, float y);
void puppet_rig_release_limb(puppet_rig_t *rig, int limb);
void puppet_rig_cycle_gesture(puppet_rig_t *rig, int hand);
const char *puppet_action_name(int action);
