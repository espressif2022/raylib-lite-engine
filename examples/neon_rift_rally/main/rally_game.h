// SPDX-License-Identifier: Apache-2.0
#pragma once

/*
 * Deterministic gameplay model for Neon Rift Rally.
 *
 * The renderer is deliberately not part of this header.  The model stores the
 * car in track-local coordinates (progress along the closed spline and an
 * offset from its centre line), so a Host preview and the device renderer use
 * exactly the same simulation.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RALLY_TICK_HZ 30U
#define RALLY_DT (1.0f / 30.0f)
#define RALLY_TRACK_LENGTH 1800.0f
#define RALLY_TRACK_SEGMENT_LENGTH 12.0f
#define RALLY_TRACK_SEGMENT_COUNT 150U
#define RALLY_TRACK_SCENERY_SLOTS 2U
#define RALLY_CHECKPOINT_COUNT 4U
#define RALLY_TARGET_LAPS 3U
#define RALLY_OPPONENT_COUNT 3U
#define RALLY_TRACK_WIDTH 10.0f
#define RALLY_MAX_LATERAL 6.25f
#define RALLY_MAX_SPEED 38.0f
#define RALLY_NITRO_MAX 100.0f

typedef enum {
    RALLY_PHASE_COUNTDOWN = 0,
    RALLY_PHASE_PLAYING,
    RALLY_PHASE_FINISHED,
    RALLY_PHASE_FAILED,
} rally_phase_t;

/* Events are one-tick flags.  A single update may emit both CHECKPOINT and
 * LAP (and, on the final lap, FINISH).  The serial changes whenever any flag
 * is emitted, which gives audio/rumble consumers an edge-triggered ABI. */
typedef enum {
    RALLY_EVENT_NONE       = 0u,
    RALLY_EVENT_JUMP       = 1u << 0,
    RALLY_EVENT_LANDING    = 1u << 1,
    RALLY_EVENT_CHECKPOINT = 1u << 2,
    RALLY_EVENT_LAP        = 1u << 3,
    RALLY_EVENT_FINISH     = 1u << 4,
    RALLY_EVENT_NITRO      = 1u << 5,
    RALLY_EVENT_DRIFT      = 1u << 6,
    RALLY_EVENT_OFFTRACK   = 1u << 7,
    RALLY_EVENT_COLLISION  = 1u << 8,
    RALLY_EVENT_NEAR_MISS  = 1u << 9,
    RALLY_EVENT_COMBO      = 1u << 10,
    RALLY_EVENT_START      = 1u << 11,
    RALLY_EVENT_FAIL       = 1u << 12,
    RALLY_EVENT_DRIFT_BOOST = 1u << 13,
} rally_event_flags_t;

typedef struct {
    float throttle; /* 0..1 */
    float brake;    /* 0..1 */
    float steer;    /* -1..1, left to right */
    bool drift;
    bool nitro;
    bool jump;      /* optional manual jump; ramps also auto-launch */
} rally_input_t;

/* A point sampled in world space from track-local progress/lateral values. */
typedef struct {
    float x, y, z;
    float tangent_x, tangent_y, tangent_z;
    float right_x, right_z;
    float bank;
    float width;
} rally_track_pose_t;

/* Render-facing road ribbon sample.  Segment indices and progress wrap at
 * the same seam, so a renderer can keep a small ring of these records while
 * the car moves without accumulating world-space drift. */
typedef struct {
    uint16_t index;
    float progress;
    float local_progress;
    float center_x, center_y, center_z;
    float left_x, left_y, left_z;
    float right_x, right_y, right_z;
    float curve;
    float slope;
    float elevation;
    float width;
} rally_track_segment_t;

/* Deterministic roadside decoration anchor.  `world_*` is already sampled
 * from the same road curve; `lateral` and `anchor_progress` remain available
 * for a renderer that wants to resample at a different elevation. */
typedef struct {
    uint16_t segment;
    uint8_t slot;
    uint8_t kind;
    int8_t side;
    float anchor_progress;
    float lateral;
    float world_x, world_y, world_z;
    float scale;
    uint32_t seed;
} rally_track_scenery_t;

/* Ghost/AI cars intentionally expose the same local coordinates as the
 * player.  A view can therefore render them through rally_track_sample() with
 * no second world-space simulation. */
typedef struct {
    float progress;
    float lateral;
    float speed;
    float lane;
    float phase;
    uint16_t laps_completed;
    uint8_t position;
    uint8_t collision_cooldown;
    uint8_t near_miss_cooldown;
    bool active;
    bool finished;
} rally_opponent_t;

typedef struct {
    /* Track-local motion state.  progress wraps at RALLY_TRACK_LENGTH. */
    float progress;
    float lateral;
    float lateral_velocity;
    float speed;
    float heading_error;

    /* Height is relative to the sampled track surface, not world y. */
    float height;
    float vertical_speed;
    float air_time;
    bool grounded;
    bool jump_armed;

    /* Driving feedback and resources. */
    float nitro;
    float drift_meter;
    bool drifting;
    bool nitro_active;
    bool previous_nitro_active;
    bool previous_drifting;
    bool offtrack;
    uint8_t collision_ticks;
    uint8_t drift_boost_ticks;
    float impact_speed;
    float drift_boost_speed;

    /* Race progression. */
    uint32_t tick;
    uint16_t laps_completed;
    uint8_t next_checkpoint;
    uint8_t checkpoint_mask;
    uint8_t missed_checkpoints;
    uint16_t countdown_ticks;
    uint16_t offtrack_ticks;
    uint8_t position;
    uint8_t integrity;
    rally_phase_t phase;

    /* Risk/reward score loop.  Near misses and drift cash out through combo;
     * crashes reset it, making a fast line more valuable but more dangerous. */
    uint32_t score;
    uint16_t combo;
    uint16_t combo_timer;
    float risk_meter;

    rally_input_t input;
    bool previous_jump;

    /* Semantic event output.  event_flags is cleared at the beginning of the
     * next update; event_serial is monotonic and never cleared by a consumer. */
    uint32_t event_flags;
    uint32_t event_serial;
    uint32_t last_event;

    rally_opponent_t opponents[RALLY_OPPONENT_COUNT];
} rally_game_t;

void rally_reset(rally_game_t *game);
void rally_start(rally_game_t *game);
void rally_set_input(rally_game_t *game, rally_input_t input);
void rally_set_controls(rally_game_t *game, float throttle, float brake,
                        float steer, bool drift, bool nitro);
void rally_set_jump(rally_game_t *game, bool jump);
void rally_fail(rally_game_t *game);
void rally_update(rally_game_t *game);

/* Event accessors are useful to modules that route sound and haptics. */
uint32_t rally_events(const rally_game_t *game);
uint32_t rally_take_events(rally_game_t *game);

/* Closed-track geometry.  `progress` is wrapped internally. */
float rally_track_length(void);
float rally_track_segment_length(void);
unsigned rally_track_segment_count(void);
float rally_checkpoint_distance(unsigned checkpoint);
bool rally_track_sample(float progress, float lateral, rally_track_pose_t *out);
bool rally_track_segment_sample(float progress, rally_track_segment_t *out);
bool rally_track_scenery(unsigned segment, unsigned slot,
                         rally_track_scenery_t *out);

uint32_t rally_state_hash(const rally_game_t *game);
