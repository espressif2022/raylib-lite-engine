// SPDX-License-Identifier: Apache-2.0
#include "rally_game.h"

#include <math.h>
#include <string.h>

#define RALLY_PI 3.14159265358979323846f
#define RALLY_TAU (2.0f * RALLY_PI)
#define RALLY_GRAVITY 24.0f
#define RALLY_JUMP_SPEED 9.0f
#define RALLY_AUTO_JUMP_SPEED 11.0f
#define RALLY_NITRO_DRAIN 28.0f
#define RALLY_NITRO_RECHARGE 8.0f
#define RALLY_COUNTDOWN_TICKS 90U
#define RALLY_OFFTRACK_FAIL_TICKS 300U
#define RALLY_COLLISION_DAMAGE 12U
#define RALLY_COMBO_TIMEOUT 90U
#define RALLY_DRIFT_BOOST_MIN 1.0f
#define RALLY_DRIFT_BOOST_MAX 3.5f
#define RALLY_DRIFT_BOOST_TIME 18U

/* Checkpoints are deliberately farther apart than the maximum movement in a
 * tick.  This keeps crossing deterministic and makes a missed checkpoint
 * visible instead of silently crediting a shortcut. */
static const float s_checkpoints[RALLY_CHECKPOINT_COUNT - 1U] = {
    360.0f, 760.0f, 1180.0f,
};

static const float s_opponent_speeds[RALLY_OPPONENT_COUNT] = {
    33.2f, 31.4f, 29.8f, 32.4f, 30.6f, 28.9f,
};
static const float s_opponent_lanes[RALLY_OPPONENT_COUNT] = {
    -3.5f, -2.1f, -0.7f, 0.7f, 2.1f, 3.5f,
};

static float clampf(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static float wrap_progress(float value)
{
    while (value < 0.0f) value += RALLY_TRACK_LENGTH;
    while (value >= RALLY_TRACK_LENGTH) value -= RALLY_TRACK_LENGTH;
    return value;
}

static float track_ramp(float progress)
{
    /* A short translucent-looking jump ramp in local track space. */
    if (progress < 665.0f || progress >= 780.0f) return 0.0f;
    const float t = (progress - 665.0f) / 115.0f;
    return 1.55f * sinf(RALLY_PI * t);
}

static float track_ramp_derivative(float progress)
{
    if (progress < 665.0f || progress >= 780.0f) return 0.0f;
    const float t = (progress - 665.0f) / 115.0f;
    return 1.55f * RALLY_PI / 115.0f * cosf(RALLY_PI * t);
}

static float track_height(float progress)
{
    const float u = RALLY_TAU * wrap_progress(progress) / RALLY_TRACK_LENGTH;
    return 1.2f + 1.0f * sinf(u * 2.0f) + 0.55f * sinf(u * 5.0f) +
           track_ramp(progress);
}

static float track_height_derivative(float progress)
{
    const float u = RALLY_TAU * wrap_progress(progress) / RALLY_TRACK_LENGTH;
    const float du = RALLY_TAU / RALLY_TRACK_LENGTH;
    return 2.0f * du * cosf(u * 2.0f) + 2.75f * du * cosf(u * 5.0f) +
           track_ramp_derivative(progress);
}

static void emit(rally_game_t *game, uint32_t flags)
{
    game->event_flags |= flags;
}

static void track_center(unsigned course_id, float progress, float *x, float *y, float *z,
                         float *tx, float *ty, float *tz, float *bank)
{
    const float wrapped = wrap_progress(progress);
    const float u = RALLY_TAU * wrapped / RALLY_TRACK_LENGTH;
    const float du = RALLY_TAU / RALLY_TRACK_LENGTH;

    /* An intentionally non-circular course: the two sine harmonics produce a
     * broad hairpin and an S bend while remaining cheap to sample. */
    float dx, dz;
    if (course_id % 3U == 1U) {
        *x = 54.0f * sinf(u) + 5.0f * sinf(2.0f * u);
        *z = 70.0f * cosf(u) + 6.0f * sinf(3.0f * u);
        dx = (54.0f * cosf(u) + 10.0f * cosf(2.0f * u)) * du;
        dz = (-70.0f * sinf(u) + 18.0f * cosf(3.0f * u)) * du;
    } else if (course_id % 3U == 2U) {
        *x = 32.0f * sinf(u) + 18.0f * sinf(3.0f * u);
        *z = 45.0f * cosf(u) + 14.0f * sinf(4.0f * u);
        dx = (32.0f * cosf(u) + 54.0f * cosf(3.0f * u)) * du;
        dz = (-45.0f * sinf(u) + 56.0f * cosf(4.0f * u)) * du;
    } else {
        *x = 38.0f * sinf(u) + 9.0f * sinf(2.0f * u);
        *z = 56.0f * cosf(u) + 8.0f * sinf(3.0f * u);
        dx = (38.0f * cosf(u) + 18.0f * cosf(2.0f * u)) * du;
        dz = (-56.0f * sinf(u) + 24.0f * cosf(3.0f * u)) * du;
    }
    *y = track_height(wrapped);
    float dy = track_height_derivative(wrapped);
    float length = sqrtf(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f) length = 1.0f;
    *tx = dx / length;
    *ty = dy / length;
    *tz = dz / length;
    *bank = 0.18f * sinf(2.0f * u) + 0.08f * sinf(5.0f * u);
}

float rally_track_length(void)
{
    return RALLY_TRACK_LENGTH;
}

float rally_track_segment_length(void)
{
    return RALLY_TRACK_SEGMENT_LENGTH;
}

unsigned rally_track_segment_count(void)
{
    return RALLY_TRACK_SEGMENT_COUNT;
}

float rally_checkpoint_distance(unsigned checkpoint)
{
    if (checkpoint < RALLY_CHECKPOINT_COUNT - 1U) return s_checkpoints[checkpoint];
    return RALLY_TRACK_LENGTH;
}

bool rally_track_sample_course(unsigned course_id, float progress, float lateral,
                               rally_track_pose_t *out)
{
    if (!out) return false;
    float cx, cy, cz, tx, ty, tz, bank;
    track_center(course_id, progress, &cx, &cy, &cz, &tx, &ty, &tz, &bank);
    /* Right is horizontal, which makes local lateral motion independent from
     * pitch on a hill and avoids roll accumulating in gameplay coordinates. */
    const float right_x = -tz;
    const float right_z = tx;
    out->x = cx + right_x * lateral;
    out->y = cy;
    out->z = cz + right_z * lateral;
    out->tangent_x = tx;
    out->tangent_y = ty;
    out->tangent_z = tz;
    out->right_x = right_x;
    out->right_z = right_z;
    out->bank = bank;
    out->width = RALLY_TRACK_WIDTH;
    return true;
}

bool rally_track_sample(float progress, float lateral, rally_track_pose_t *out)
{
    return rally_track_sample_course(0U, progress, lateral, out);
}

bool rally_track_segment_sample(float progress, rally_track_segment_t *out)
{
    if (!out) return false;
    const float wrapped = wrap_progress(progress);
    unsigned index = (unsigned)(wrapped / RALLY_TRACK_SEGMENT_LENGTH);
    if (index >= RALLY_TRACK_SEGMENT_COUNT) index = RALLY_TRACK_SEGMENT_COUNT - 1U;
    const float segment_progress = (float)index * RALLY_TRACK_SEGMENT_LENGTH;
    const float local_progress = wrapped - segment_progress;
    rally_track_pose_t center, left, right;
    rally_track_sample(wrapped, 0.0f, &center);
    rally_track_sample(wrapped, -RALLY_TRACK_WIDTH * 0.5f, &left);
    rally_track_sample(wrapped, RALLY_TRACK_WIDTH * 0.5f, &right);

    /* Tangent deltas are a compact curvature estimate.  Sampling through the
     * public pose function also makes the 0/L seam follow exactly the same
     * wrapping rules as the gameplay model. */
    const float half_step = RALLY_TRACK_SEGMENT_LENGTH * 0.5f;
    rally_track_pose_t previous, next;
    rally_track_sample(wrapped - half_step, 0.0f, &previous);
    rally_track_sample(wrapped + half_step, 0.0f, &next);
    const float reciprocal_step = 1.0f / (2.0f * half_step);

    out->index = (uint16_t)index;
    out->progress = wrapped;
    out->local_progress = local_progress;
    out->center_x = center.x;
    out->center_y = center.y;
    out->center_z = center.z;
    out->left_x = left.x;
    out->left_y = left.y;
    out->left_z = left.z;
    out->right_x = right.x;
    out->right_y = right.y;
    out->right_z = right.z;
    out->curve = (next.tangent_x - previous.tangent_x) * reciprocal_step;
    out->slope = (next.y - previous.y) * reciprocal_step;
    out->elevation = center.y;
    out->width = RALLY_TRACK_WIDTH;
    return true;
}

static uint32_t scenery_hash(uint32_t value)
{
    /* A small integer mixer avoids a PRNG stream or mutable decoration state.
     * The result is stable for every visit to a segment, including replays. */
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float scenery_unit(uint32_t seed)
{
    return (float)(seed & 0xffffu) / 65535.0f;
}

bool rally_track_scenery_course(unsigned course_id, unsigned segment, unsigned slot,
                                rally_track_scenery_t *out)
{
    if (!out || slot >= RALLY_TRACK_SCENERY_SLOTS) return false;
    const unsigned wrapped_segment = segment % RALLY_TRACK_SEGMENT_COUNT;
    const uint32_t seed = scenery_hash(0x9e3779b9u ^
                                       (uint32_t)wrapped_segment * 0x45d9f3bu ^
                                       (uint32_t)slot * 0x27d4eb2du);
    /* Slot zero is always populated.  Slot one is sparse, which keeps the
     * pseudo-3D view from filling every horizon column with props. */
    if (slot == 1U && (seed & 3u) == 0u) return false;

    const int8_t side = (seed & 0x10u) ? 1 : -1;
    const float offset = RALLY_TRACK_WIDTH * 0.5f + 1.8f +
                         scenery_unit(seed >> 8) * 3.4f;
    const float anchor_progress = (float)wrapped_segment * RALLY_TRACK_SEGMENT_LENGTH +
                                  (0.18f + scenery_unit(seed >> 16) * 0.64f) *
                                  RALLY_TRACK_SEGMENT_LENGTH;
    rally_track_pose_t pose;
    rally_track_sample_course(course_id, anchor_progress, (float)side * offset, &pose);
    out->segment = (uint16_t)wrapped_segment;
    out->slot = (uint8_t)slot;
    out->kind = (uint8_t)(1u + ((seed >> 20) % 5u));
    out->side = side;
    out->anchor_progress = anchor_progress;
    out->lateral = (float)side * offset;
    out->world_x = pose.x;
    out->world_y = pose.y;
    out->world_z = pose.z;
    out->scale = 0.72f + scenery_unit(seed >> 3) * 0.92f;
    out->seed = seed;
    return true;
}

bool rally_track_scenery(unsigned segment, unsigned slot,
                         rally_track_scenery_t *out)
{
    return rally_track_scenery_course(0U, segment, slot, out);
}

static bool in_launch_zone(float progress)
{
    return progress >= 665.0f && progress < 780.0f;
}

static void launch(rally_game_t *game)
{
    if (!game->grounded) return;
    game->grounded = false;
    game->height = 0.04f;
    game->vertical_speed = RALLY_JUMP_SPEED;
    game->air_time = 0.0f;
    game->jump_armed = false;
    emit(game, RALLY_EVENT_JUMP);
}

static void process_checkpoint_segment(rally_game_t *game, float from, float to)
{
    while (game->next_checkpoint < RALLY_CHECKPOINT_COUNT - 1U) {
        const float target = s_checkpoints[game->next_checkpoint];
        if (target <= from || target > to) break;
        game->checkpoint_mask |= (uint8_t)(1U << game->next_checkpoint);
        ++game->next_checkpoint;
        emit(game, RALLY_EVENT_CHECKPOINT);
    }
}

static void advance_race(rally_game_t *game, float old_progress, float distance)
{
    if (distance <= 0.0f) return;
    const float end = old_progress + distance;
    if (end < RALLY_TRACK_LENGTH) {
        process_checkpoint_segment(game, old_progress, end);
        return;
    }

    process_checkpoint_segment(game, old_progress, RALLY_TRACK_LENGTH);
    if (game->next_checkpoint == RALLY_CHECKPOINT_COUNT - 1U) {
        ++game->laps_completed;
        emit(game, RALLY_EVENT_LAP);
        if (game->laps_completed >= RALLY_TARGET_LAPS) {
            game->phase = RALLY_PHASE_FINISHED;
            emit(game, RALLY_EVENT_FINISH);
        }
    } else {
        /* Crossing the line without all three gates is a lap miss.  It does
         * not freeze the car; the next lap starts from checkpoint zero. */
        if (game->next_checkpoint != 0U && game->missed_checkpoints < 255U)
            ++game->missed_checkpoints;
    }
    game->next_checkpoint = 0U;
    game->checkpoint_mask = 0U;
    process_checkpoint_segment(game, 0.0f, end - RALLY_TRACK_LENGTH);
}

static float race_distance(float progress, uint16_t laps)
{
    return (float)laps * RALLY_TRACK_LENGTH + progress;
}

static float loop_gap(float a, float b)
{
    float gap = fabsf(a - b);
    if (gap > RALLY_TRACK_LENGTH * 0.5f) gap = RALLY_TRACK_LENGTH - gap;
    return gap;
}

static void update_opponents(rally_game_t *game)
{
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        rally_opponent_t *opponent = &game->opponents[i];
        if (!opponent->active || opponent->finished) continue;
        if (opponent->collision_cooldown) --opponent->collision_cooldown;
        if (opponent->near_miss_cooldown) --opponent->near_miss_cooldown;

        /* A deterministic pace wave makes the cars look less like three
         * copies, while keeping their input entirely replayable. */
        const float wave = 1.35f * sinf(opponent->phase +
                                        (float)game->tick * (0.017f + i * 0.003f));
        const float player_distance = race_distance(game->progress,
                                                    game->laps_completed);
        const float opponent_distance = race_distance(opponent->progress,
                                                       opponent->laps_completed);
        const float gap = player_distance - opponent_distance;
        /* Gentle catch-up gives a trailing AI a reason to stay in the race,
         * but caps the rubber band below the player's boosted top speed. */
        const float catch_up = clampf(gap * 0.008f, 0.0f, 3.0f);
        const float leader_brake = clampf(-gap * 0.006f, 0.0f, 1.5f);
        const float target_speed = s_opponent_speeds[i] + wave + catch_up - leader_brake;
        opponent->speed += (target_speed - opponent->speed) * 2.7f * RALLY_DT;
        opponent->lane = s_opponent_lanes[i] +
                         0.7f * sinf(opponent->phase +
                                    (float)game->tick * (0.021f + i * 0.004f));
        opponent->lateral += (opponent->lane - opponent->lateral) *
                             fminf(1.0f, 2.0f * RALLY_DT);
        opponent->progress += opponent->speed * RALLY_DT;
        if (opponent->progress >= RALLY_TRACK_LENGTH) {
            opponent->progress -= RALLY_TRACK_LENGTH;
            ++opponent->laps_completed;
            if (opponent->laps_completed >= RALLY_TARGET_LAPS) {
                opponent->finished = true;
                opponent->speed = 0.0f;
            }
        }
    }
}

static void update_rank(rally_game_t *game)
{
    const float player_distance = race_distance(game->progress,
                                                game->laps_completed);
    unsigned rank = 1U;
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        const rally_opponent_t *opponent = &game->opponents[i];
        const float opponent_distance = race_distance(opponent->progress,
                                                       opponent->laps_completed);
        if (opponent_distance > player_distance + 0.01f) ++rank;
    }
    game->position = (uint8_t)rank;
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        const float opponent_distance = race_distance(game->opponents[i].progress,
                                                       game->opponents[i].laps_completed);
        unsigned opponent_rank = 1U;
        for (unsigned j = 0; j < RALLY_OPPONENT_COUNT; ++j) {
            if (i == j) continue;
            const float other_distance = race_distance(game->opponents[j].progress,
                                                       game->opponents[j].laps_completed);
            if (other_distance > opponent_distance + 0.01f) ++opponent_rank;
        }
        if (player_distance > opponent_distance + 0.01f) ++opponent_rank;
        game->opponents[i].position = (uint8_t)opponent_rank;
    }
}

static void award_combo(rally_game_t *game, uint32_t base_score)
{
    if (game->combo == 0U) game->combo = 1U;
    else if (game->combo < 999U) ++game->combo;
    game->combo_timer = RALLY_COMBO_TIMEOUT;
    game->score += base_score * game->combo;
    emit(game, RALLY_EVENT_COMBO);
}

static void update_risk(rally_game_t *game)
{
    if (game->combo_timer) --game->combo_timer;
    else game->combo = 0U;
    if (game->drifting) {
        game->risk_meter = clampf(game->risk_meter +
                                  fabsf(game->lateral_velocity) * 2.3f * RALLY_DT,
                                  0.0f, 100.0f);
    }
    if (game->previous_drifting && !game->drifting && game->risk_meter >= 8.0f) {
        const uint32_t payout = (uint32_t)(game->risk_meter * 4.0f);
        const float boost = clampf(game->risk_meter * 0.045f,
                                   RALLY_DRIFT_BOOST_MIN, RALLY_DRIFT_BOOST_MAX);
        game->speed = clampf(game->speed + boost, 0.0f,
                             RALLY_MAX_SPEED + (game->nitro_active ? 13.0f : 0.0f));
        game->drift_boost_speed = boost * 8.0f;
        game->drift_boost_ticks = RALLY_DRIFT_BOOST_TIME;
        award_combo(game, payout);
        emit(game, RALLY_EVENT_DRIFT_BOOST);
        game->risk_meter = 0.0f;
    }
}

static void update_interactions(rally_game_t *game)
{
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        rally_opponent_t *opponent = &game->opponents[i];
        if (!opponent->active) continue;
        const float along = loop_gap(game->progress, opponent->progress);
        const float across = fabsf(game->lateral - opponent->lateral);
        if (along < 2.0f && across < 1.5f && opponent->collision_cooldown == 0U) {
            /* The cooldown is long enough for the two cars to separate; an
             * overlap must not become a machine-gun of damage. */
            opponent->collision_cooldown = 120U;
            game->impact_speed = game->speed;
            game->collision_ticks = 14U;
            game->speed *= 0.62f;
            game->lateral_velocity -= (game->lateral >= opponent->lateral ? 1.0f : -1.0f) * 2.2f;
            game->integrity = game->integrity > RALLY_COLLISION_DAMAGE
                                  ? (uint8_t)(game->integrity - RALLY_COLLISION_DAMAGE)
                                  : 0U;
            game->combo = 0U;
            game->combo_timer = 0U;
            game->risk_meter = 0.0f;
            emit(game, RALLY_EVENT_COLLISION);
            if (game->integrity == 0U) {
                game->phase = RALLY_PHASE_FAILED;
                emit(game, RALLY_EVENT_FAIL);
            }
        } else if (along >= 2.0f && along < 8.0f && across < 2.0f &&
                   opponent->near_miss_cooldown == 0U) {
            opponent->near_miss_cooldown = 30U;
            award_combo(game, 40U);
            emit(game, RALLY_EVENT_NEAR_MISS);
        }
    }
}

void rally_reset(rally_game_t *game)
{
    if (!game) return;
    memset(game, 0, sizeof(*game));
    game->grounded = true;
    game->jump_armed = true;
    game->nitro = RALLY_NITRO_MAX;
    game->countdown_ticks = RALLY_COUNTDOWN_TICKS;
    game->integrity = 100U;
    game->position = RALLY_OPPONENT_COUNT + 1U;
    game->phase = RALLY_PHASE_COUNTDOWN;
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        game->opponents[i].progress = 9.0f * (float)(i + 1U);
        game->opponents[i].lateral = s_opponent_lanes[i];
        game->opponents[i].lane = s_opponent_lanes[i];
        game->opponents[i].phase = 0.8f * (float)i;
        game->opponents[i].active = true;
        game->opponents[i].position = (uint8_t)(i + 1U);
    }
}

void rally_start(rally_game_t *game)
{
    if (!game) return;
    if (game->phase == RALLY_PHASE_FINISHED || game->phase == RALLY_PHASE_FAILED)
        rally_reset(game);
}

void rally_set_input(rally_game_t *game, rally_input_t input)
{
    if (!game) return;
    game->input.throttle = clampf(input.throttle, 0.0f, 1.0f);
    game->input.brake = clampf(input.brake, 0.0f, 1.0f);
    game->input.steer = clampf(input.steer, -1.0f, 1.0f);
    game->input.drift = input.drift;
    game->input.nitro = input.nitro;
    game->input.jump = input.jump;
}

void rally_set_controls(rally_game_t *game, float throttle, float brake,
                        float steer, bool drift, bool nitro)
{
    rally_input_t input = { throttle, brake, steer, drift, nitro, false };
    rally_set_input(game, input);
}

void rally_set_jump(rally_game_t *game, bool jump)
{
    if (game) game->input.jump = jump;
}

void rally_fail(rally_game_t *game)
{
    if (!game || game->phase == RALLY_PHASE_FINISHED ||
        game->phase == RALLY_PHASE_FAILED)
        return;
    game->phase = RALLY_PHASE_FAILED;
    game->integrity = 0U;
    game->event_flags |= RALLY_EVENT_FAIL;
    ++game->event_serial;
    game->last_event = RALLY_EVENT_FAIL;
}

void rally_update(rally_game_t *game)
{
    if (!game || game->phase == RALLY_PHASE_FINISHED ||
        game->phase == RALLY_PHASE_FAILED)
        return;
    game->event_flags = RALLY_EVENT_NONE;

    if (game->phase == RALLY_PHASE_COUNTDOWN) {
        if (game->countdown_ticks) --game->countdown_ticks;
        ++game->tick;
        if (game->countdown_ticks == 0U) {
            game->phase = RALLY_PHASE_PLAYING;
            emit(game, RALLY_EVENT_START);
            ++game->event_serial;
            game->last_event = game->event_flags;
        }
        return;
    }

    const float old_progress = game->progress;
    const float dt = RALLY_DT;
    const float throttle = game->input.throttle;
    const float brake = game->input.brake;
    if (game->collision_ticks) {
        --game->collision_ticks;
        game->heading_error *= 0.82f;
        game->lateral_velocity *= 0.90f;
        game->speed *= 0.985f;
    }
    if (game->drift_boost_ticks) {
        --game->drift_boost_ticks;
        game->speed += game->drift_boost_speed * dt;
        game->drift_boost_speed *= 0.88f;
    }
    const bool wants_nitro = game->input.nitro && throttle > 0.05f &&
                             game->nitro > 0.0f && game->speed > 2.0f;
    game->nitro_active = wants_nitro;
    if (game->nitro_active) {
        game->nitro -= RALLY_NITRO_DRAIN * dt;
        if (game->nitro <= 0.0f) {
            game->nitro = 0.0f;
            game->nitro_active = false;
        }
    } else {
        game->nitro = clampf(game->nitro + RALLY_NITRO_RECHARGE * dt,
                             0.0f, RALLY_NITRO_MAX);
    }
    if (game->nitro_active && !game->previous_nitro_active)
        emit(game, RALLY_EVENT_NITRO);

    const bool wants_drift = game->input.drift && game->speed > 5.0f &&
                             game->grounded;
    game->drifting = wants_drift;
    if (game->drifting && !game->previous_drifting && !game->offtrack)
        emit(game, RALLY_EVENT_DRIFT);

    /* Fixed-step longitudinal model.  The quadratic drag is intentionally
     * simple enough to behave identically on the Host and on the MCU. */
    const float drive = throttle * (game->nitro_active ? 31.0f : 22.0f);
    const float braking = brake * 35.0f;
    const float drag = 1.35f + game->speed * 0.055f;
    game->speed += (drive - braking - drag) * dt;
    game->speed = clampf(game->speed, 0.0f,
                         RALLY_MAX_SPEED + (game->nitro_active ? 13.0f : 0.0f));

    /* Fine control around center, bounded lateral speed and faster release.
     * Motion and camera consume the same command, so a raw input step cannot
     * snap the camera independently of the motorcycle's response. */
    const float speed_ratio = clampf(game->speed / RALLY_MAX_SPEED, 0.0f, 1.0f);
    const float speed_steer = 1.08f - 0.52f * speed_ratio;
    const float low_speed_turn = game->speed > 0.5f
                                     ? (1.0f - speed_ratio) * 2.2f
                                     : 0.0f;
    float wanted_steering = game->input.steer *
                            (.40f + .60f * fabsf(game->input.steer));
    float steering_rate = fabsf(wanted_steering) < fabsf(game->steering) ||
                          wanted_steering * game->steering < 0.0f ? 8.0f : 3.0f;
    game->steering += clampf(wanted_steering-game->steering,
                             -steering_rate*dt, steering_rate*dt);
    const float drift_steer = game->drifting ? 1.10f : 1.0f;
    float steering_limit = (game->speed * .30f * speed_steer +
                            low_speed_turn * .65f) * drift_steer;
    float target_lateral_velocity = game->steering * steering_limit;
    /* A fast motorcycle carries momentum through a bend.  The road does not
     * automatically steer it: without counter-steering, curvature pushes it
     * toward the outside edge.  Compare tangents in the local right axis so
     * the sign stays correct around the closed track. */
    rally_track_pose_t bend, ahead;
    rally_track_sample_course(game->course_id, game->progress, 0.0f, &bend);
    rally_track_sample_course(game->course_id, game->progress + 6.0f, 0.0f, &ahead);
    float turn_right = bend.right_x * ahead.tangent_x +
                       bend.right_z * ahead.tangent_z;
    float outward_velocity = turn_right * speed_ratio * speed_ratio * 55.0f;
    target_lateral_velocity -= clampf(outward_velocity,
                                      -steering_limit*.55f, steering_limit*.55f);
    if (game->collision_ticks) target_lateral_velocity *= 0.45f;
    const float steering_response = (game->drifting ? 5.0f :
                                     fabsf(game->input.steer)<.06f ? 16.0f : 12.0f) * dt;
    game->lateral_velocity += (target_lateral_velocity - game->lateral_velocity) *
                              clampf(steering_response, 0.0f, 1.0f);
    game->lateral += game->lateral_velocity * dt;
    if (game->drifting) {
        game->drift_meter = clampf(game->drift_meter +
                                   fabsf(game->lateral_velocity) * 1.8f * dt,
                                   0.0f, 100.0f);
    } else {
        game->drift_meter = clampf(game->drift_meter - 24.0f * dt, 0.0f, 100.0f);
    }
    game->lateral = clampf(game->lateral, -RALLY_MAX_LATERAL, RALLY_MAX_LATERAL);
    game->heading_error += (game->steering *
                            (game->drifting ? .24f : .14f) * speed_steer -
                            game->heading_error) * (game->drifting ? 5.0f : 8.0f) * dt;

    const bool now_offtrack = fabsf(game->lateral) > RALLY_TRACK_WIDTH * 0.5f;
    if (now_offtrack && !game->offtrack) emit(game, RALLY_EVENT_OFFTRACK);
    game->offtrack = now_offtrack;
    if (game->offtrack) {
        game->speed *= 1.0f - 2.6f * dt;
        if (game->offtrack_ticks < 65535U) ++game->offtrack_ticks;
        if (game->offtrack_ticks >= RALLY_OFFTRACK_FAIL_TICKS) {
            game->phase = RALLY_PHASE_FAILED;
            emit(game, RALLY_EVENT_FAIL);
        }
    } else {
        game->offtrack_ticks = 0U;
    }

    const float distance = game->speed * dt;
    game->progress += distance;
    advance_race(game, old_progress, distance);
    game->progress = wrap_progress(game->progress);

    if (game->progress >= 790.0f) game->jump_armed = true;
    if (game->grounded && game->input.jump && !game->previous_jump && game->speed > 2.0f)
        launch(game);
    if (game->grounded && game->jump_armed && in_launch_zone(game->progress) &&
        game->speed >= RALLY_AUTO_JUMP_SPEED)
        launch(game);

    if (!game->grounded) {
        game->vertical_speed -= RALLY_GRAVITY * dt;
        game->height += game->vertical_speed * dt;
        game->air_time += dt;
        if (game->height <= 0.0f) {
            game->height = 0.0f;
            game->grounded = true;
            game->vertical_speed = 0.0f;
            emit(game, RALLY_EVENT_LANDING);
        }
    }

    update_opponents(game);
    update_interactions(game);
    update_risk(game);
    update_rank(game);

    game->previous_jump = game->input.jump;
    game->previous_nitro_active = game->nitro_active;
    game->previous_drifting = game->drifting;
    ++game->tick;
    if (game->event_flags != RALLY_EVENT_NONE) {
        ++game->event_serial;
        game->last_event = game->event_flags;
    } else {
        game->last_event = RALLY_EVENT_NONE;
    }
}

uint32_t rally_events(const rally_game_t *game)
{
    return game ? game->event_flags : RALLY_EVENT_NONE;
}

uint32_t rally_take_events(rally_game_t *game)
{
    if (!game) return RALLY_EVENT_NONE;
    const uint32_t events = game->event_flags;
    game->event_flags = RALLY_EVENT_NONE;
    return events;
}

static uint32_t hash_mix(uint32_t hash, uint32_t value)
{
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

static uint32_t hash_float(uint32_t hash, float value)
{
    /* Quantization makes the externally visible hash independent of padding
     * bytes and stable when a renderer changes unrelated state. */
    int32_t quantized = (int32_t)(value * 1000.0f);
    return hash_mix(hash, (uint32_t)quantized);
}

uint32_t rally_state_hash(const rally_game_t *game)
{
    if (!game) return 0u;
    uint32_t hash = 0x811c9dc5u;
    hash = hash_float(hash, game->progress);
    hash = hash_float(hash, game->lateral);
    hash = hash_float(hash, game->lateral_velocity);
    hash = hash_float(hash, game->speed);
    hash = hash_float(hash, game->heading_error);
    hash = hash_float(hash, game->steering);
    hash = hash_float(hash, game->height);
    hash = hash_float(hash, game->vertical_speed);
    hash = hash_float(hash, game->air_time);
    hash = hash_float(hash, game->nitro);
    hash = hash_float(hash, game->drift_meter);
    hash = hash_float(hash, game->impact_speed);
    hash = hash_float(hash, game->drift_boost_speed);
    hash = hash_float(hash, game->risk_meter);
    hash = hash_mix(hash, game->course_id);
    hash = hash_mix(hash, game->tick);
    hash = hash_mix(hash, game->laps_completed);
    hash = hash_mix(hash, game->next_checkpoint);
    hash = hash_mix(hash, game->checkpoint_mask);
    hash = hash_mix(hash, game->missed_checkpoints);
    hash = hash_mix(hash, game->countdown_ticks);
    hash = hash_mix(hash, game->offtrack_ticks);
    hash = hash_mix(hash, game->position);
    hash = hash_mix(hash, game->integrity);
    hash = hash_mix(hash, game->collision_ticks);
    hash = hash_mix(hash, game->drift_boost_ticks);
    hash = hash_mix(hash, game->score);
    hash = hash_mix(hash, game->combo);
    hash = hash_mix(hash, game->combo_timer);
    hash = hash_mix(hash, (uint32_t)game->phase);
    hash = hash_mix(hash, game->grounded ? 1u : 0u);
    hash = hash_mix(hash, game->jump_armed ? 1u : 0u);
    hash = hash_mix(hash, game->drifting ? 1u : 0u);
    hash = hash_mix(hash, game->nitro_active ? 1u : 0u);
    hash = hash_mix(hash, game->previous_nitro_active ? 1u : 0u);
    hash = hash_mix(hash, game->previous_drifting ? 1u : 0u);
    hash = hash_mix(hash, game->offtrack ? 1u : 0u);
    hash = hash_mix(hash, game->previous_jump ? 1u : 0u);
    hash = hash_float(hash, game->input.throttle);
    hash = hash_float(hash, game->input.brake);
    hash = hash_float(hash, game->input.steer);
    hash = hash_mix(hash, game->input.drift ? 1u : 0u);
    hash = hash_mix(hash, game->input.nitro ? 1u : 0u);
    hash = hash_mix(hash, game->input.jump ? 1u : 0u);
    hash = hash_mix(hash, game->event_flags);
    hash = hash_mix(hash, game->event_serial);
    hash = hash_mix(hash, game->last_event);
    for (unsigned i = 0; i < RALLY_OPPONENT_COUNT; ++i) {
        const rally_opponent_t *opponent = &game->opponents[i];
        hash = hash_float(hash, opponent->progress);
        hash = hash_float(hash, opponent->lateral);
        hash = hash_float(hash, opponent->speed);
        hash = hash_float(hash, opponent->lane);
        hash = hash_mix(hash, opponent->laps_completed);
        hash = hash_mix(hash, opponent->position);
        hash = hash_mix(hash, opponent->collision_cooldown);
        hash = hash_mix(hash, opponent->near_miss_cooldown);
        hash = hash_mix(hash, opponent->active ? 1u : 0u);
        hash = hash_mix(hash, opponent->finished ? 1u : 0u);
    }
    return hash;
}
