// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RAYLIB_LITE_INPUT_NONE = 0,
    RAYLIB_LITE_INPUT_POINTER,
    RAYLIB_LITE_INPUT_TOUCH,
    RAYLIB_LITE_INPUT_BUTTON,
    RAYLIB_LITE_INPUT_JOYSTICK,
    RAYLIB_LITE_INPUT_IMU,
    RAYLIB_LITE_INPUT_ATTACHED,
    RAYLIB_LITE_INPUT_DETACHED,
} raylib_lite_input_type_t;

typedef struct {
    raylib_lite_input_type_t type;
    int32_t x;
    int32_t y;
    int32_t value;
    bool pressed;
    uint64_t timestamp_us;
} raylib_lite_input_event_t;

typedef struct {
    void *context;
    void (*lock)(void *context);
    void (*unlock)(void *context);
} raylib_lite_input_sync_t;

/* A bounded FIFO backed by caller-owned storage. The queue never allocates.
 * Supplying both sync callbacks makes push, poll and inspectors safe for
 * multiple producers and consumers; supplying neither requires every call to
 * be externally serialized. A half-populated sync pair is invalid.
 *
 * The object is not copyable. init is one-shot: before reinitializing, reset,
 * deinit, or releasing storage/sync context, the owner must stop and join all
 * callers. The injected callbacks only bracket a bounded metadata/event-copy
 * critical section; the queue performs no retry, sleep, allocation, or other
 * user callback while locked. When full, push returns BUSY, preserves all
 * queued events, and increments dropped_events exactly once. */
typedef struct {
    raylib_lite_input_event_t *events;
    size_t capacity;
    size_t read_index;
    size_t count;
    uint32_t dropped_events;
    raylib_lite_input_sync_t sync;
    bool initialized;
} raylib_lite_input_queue_t;

raylib_lite_result_t raylib_lite_input_queue_init(
    raylib_lite_input_queue_t *queue,
    raylib_lite_input_event_t *storage,
    size_t capacity,
    const raylib_lite_input_sync_t *sync);
void raylib_lite_input_queue_reset(raylib_lite_input_queue_t *queue);
void raylib_lite_input_queue_deinit(raylib_lite_input_queue_t *queue);
raylib_lite_result_t raylib_lite_input_push(
    raylib_lite_input_queue_t *queue,
    const raylib_lite_input_event_t *event);
bool raylib_lite_input_poll(raylib_lite_input_queue_t *queue,
                            raylib_lite_input_event_t *out_event);
uint32_t raylib_lite_input_dropped(raylib_lite_input_queue_t *queue);
size_t raylib_lite_input_count(raylib_lite_input_queue_t *queue);

#ifdef __cplusplus
}
#endif
