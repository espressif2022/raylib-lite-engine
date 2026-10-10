// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TOMB_NO_ROOM 0xFFU
#define TOMB_SECTOR_SIZE 1.0f
#define TOMB_TEX_SIZE 64
#define TOMB_TEX_COLUMNS 5
#define TOMB_TEX_COUNT 10

typedef struct {
    float x, y, z;
} tomb_vec3_t;

typedef struct {
    float floor[4];
    float ceiling[4];
    bool solid;
} tomb_sector_t;

typedef struct {
    uint16_t vertex[4];
    uint16_t u[4];
    uint16_t v[4];
    uint8_t light[4];
    uint8_t texture;
} tomb_face_t;

typedef struct {
    tomb_vec3_t corners[4];
    uint8_t target_room;
} tomb_portal_t;

typedef struct {
    float origin_x;
    float origin_z;
    uint8_t width;
    uint8_t depth;
    uint8_t ambient;
    const tomb_sector_t *sectors;
    uint16_t sector_count;
    const tomb_vec3_t *vertices;
    uint16_t vertex_count;
    const tomb_face_t *faces;
    uint16_t face_count;
    const tomb_portal_t *portals;
    uint16_t portal_count;
} tomb_room_t;

typedef struct {
    const tomb_room_t *rooms;
    uint8_t room_count;
    tomb_vec3_t start;
    float start_yaw;
    uint8_t start_room;
} tomb_level_t;

const tomb_level_t *tomb_level(void);
uint8_t tomb_room_at(float x, float z, uint8_t hint);
bool tomb_heights_at(uint8_t room, float x, float z, float *floor_out, float *ceiling_out);
