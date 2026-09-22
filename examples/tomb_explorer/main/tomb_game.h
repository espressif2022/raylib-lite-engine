// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "tomb_level.h"

#define TOMB_DT (1.0f / 30.0f)
#define TOMB_WALK_SPEED 2.4f
#define TOMB_STEP_UP 0.36f
#define TOMB_GRAVITY 11.0f
#define TOMB_JUMP_SPEED 4.4f
#define TOMB_RADIUS 0.25f
#define TOMB_HEADROOM 1.75f
#define TOMB_CAMERA_HEIGHT 1.35f
#define TOMB_CAMERA_DIST 2.8f
#define TOMB_CAMERA_MIN 0.6f
#define TOMB_MOVE_X 82
#define TOMB_MOVE_Y 392
#define TOMB_MOVE_R 64
#define TOMB_JUMP_X 398
#define TOMB_JUMP_Y 392
#define TOMB_JUMP_R 56
#define TOMB_LOOK_MIN_X 240

typedef struct {
    float x, y, z;
    float yaw;
    float speed;
    float vertical_speed;
    bool grounded;
    uint8_t room;
    float walk_phase;
    float walk_weight;
    float crouch;
    float airborne;
    float landing;
    float camera_yaw;
    float camera_pitch;
    float cam_x, cam_y, cam_z;
    uint8_t camera_room;
    float forward;
    float strafe;
    float orbit;
    float tilt;
    bool jump;
    uint32_t tick;
} tomb_game_t;

void tomb_reset(tomb_game_t *game);
void tomb_set_stick(tomb_game_t *game, float x, float y);
void tomb_set_look(tomb_game_t *game, float orbit, float tilt);
void tomb_set_jump(tomb_game_t *game, bool jump);
void tomb_update(tomb_game_t *game);
uint32_t tomb_state_hash(const tomb_game_t *game);
bool tomb_in_move_zone(int x, int y);
bool tomb_in_move_capture(int x, int y);
bool tomb_in_jump_zone(int x, int y);
