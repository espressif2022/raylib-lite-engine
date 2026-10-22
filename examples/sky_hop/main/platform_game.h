// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLATFORM_LEVEL_COUNT 4
#define PLATFORM_COIN_COUNT 16
#define PLATFORM_ENEMY_COUNT 8
#define PLATFORM_POINTER_COUNT 2
#define PLATFORM_SPRING_COUNT 4
#define PLATFORM_INVULNERABLE_TICKS 54U
#define PLATFORM_STOMP_CHAIN_MAX 4U
#define PLATFORM_VIEW_WIDTH 480.0f

typedef enum {
    PLATFORM_START,
    PLATFORM_PLAYING,
    PLATFORM_PAUSED,
    PLATFORM_LEVEL_CLEAR,
    PLATFORM_WON,
    PLATFORM_GAME_OVER,
} platform_phase_t;

typedef enum {
    PLATFORM_ACTION_LEFT,
    PLATFORM_ACTION_RIGHT,
    PLATFORM_ACTION_JUMP,
    PLATFORM_ACTION_PAUSE,
    PLATFORM_ACTION_RESTART,
} platform_action_t;

/* Events accumulate until platform_game_take_events() consumes them. */
typedef enum {
    PLATFORM_EVENT_START = 1U << 0,
    PLATFORM_EVENT_JUMP = 1U << 1,
    PLATFORM_EVENT_LAND = 1U << 2,
    PLATFORM_EVENT_COIN = 1U << 3,
    PLATFORM_EVENT_STOMP = 1U << 4,
    PLATFORM_EVENT_SPRING = 1U << 5,
    PLATFORM_EVENT_CHECKPOINT = 1U << 6,
    PLATFORM_EVENT_HURT = 1U << 7,
    PLATFORM_EVENT_LEVEL_CLEAR = 1U << 8,
    PLATFORM_EVENT_WON = 1U << 9,
    PLATFORM_EVENT_GAME_OVER = 1U << 10,
    PLATFORM_EVENT_PAUSE = 1U << 11,
    PLATFORM_EVENT_RESUME = 1U << 12,
} platform_event_t;

typedef struct {
    float x, y;
    bool collected;
} platform_coin_t;

typedef struct {
    float x, y, left, right, speed;
    bool active;
} platform_enemy_t;

typedef struct {
    bool active;
    int32_t track_id;
    float x, y;
} platform_pointer_t;

typedef struct {
    platform_phase_t phase;
    float player_x, player_y, velocity_x, velocity_y;
    float camera_x;
    float view_width; /* visible course width in world units */
    float respawn_x;
    bool move_left, move_right, jump_held, grounded, facing_left;
    bool checkpoint_active;
    uint8_t coyote_frames;
    uint8_t jump_buffer;
    uint8_t coin_chain;
    uint8_t coin_chain_timer;
    uint8_t stomp_chain;
    uint8_t invulnerable_ticks;
    uint8_t air_ticks;
    uint8_t land_air_ticks;
    uint8_t spring_ticks[PLATFORM_SPRING_COUNT];
    uint32_t tick;
    uint32_t phase_tick;
    uint32_t score;
    uint32_t events;
    uint32_t event_serial;
    uint16_t last_gain;
    float last_gain_x, last_gain_y;
    float hit_x, hit_y;
    uint8_t lives;
    uint8_t level;
    uint8_t coin_count;
    uint8_t coins_collected;
    uint8_t enemy_count;
    uint8_t stomps;
    platform_coin_t coins[PLATFORM_COIN_COUNT];
    platform_enemy_t enemies[PLATFORM_ENEMY_COUNT];
    platform_pointer_t pointers[PLATFORM_POINTER_COUNT];
} platform_game_t;

typedef struct { float x, y, width, height; } platform_block_t;
typedef struct { float x, y, width; } platform_spring_t;

void platform_game_reset(platform_game_t *game);
void platform_game_set_action(platform_game_t *game, platform_action_t action,
                              bool pressed);
void platform_game_set_view_width(platform_game_t *game, float width);
void platform_game_set_pointer(platform_game_t *game, float x, float y, bool pressed,
                              int32_t track_id);
void platform_game_update(platform_game_t *game);
uint32_t platform_game_take_events(platform_game_t *game);
const platform_block_t *platform_game_blocks(const platform_game_t *game,
                                              size_t *count);
float platform_game_world_width(const platform_game_t *game);
float platform_game_finish_x(const platform_game_t *game);
float platform_game_checkpoint_x(const platform_game_t *game);
const platform_spring_t *platform_game_springs(const platform_game_t *game,
                                               size_t *count);
const char *platform_game_level_name(const platform_game_t *game);
uint32_t platform_game_state_hash(const platform_game_t *game);
