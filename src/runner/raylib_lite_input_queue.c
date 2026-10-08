// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_input.h"

static void queue_lock(raylib_lite_input_queue_t *queue)
{
    if (queue->sync.lock) queue->sync.lock(queue->sync.context);
}

static void queue_unlock(raylib_lite_input_queue_t *queue)
{
    if (queue->sync.unlock) queue->sync.unlock(queue->sync.context);
}

raylib_lite_result_t raylib_lite_input_queue_init(
    raylib_lite_input_queue_t *queue,
    raylib_lite_input_event_t *storage,
    size_t capacity,
    const raylib_lite_input_sync_t *sync)
{
    if (!queue || !storage || capacity == 0)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    if (sync && ((!sync->lock && sync->unlock) ||
                 (sync->lock && !sync->unlock)))
        return RAYLIB_LITE_INVALID_ARGUMENT;
    *queue = (raylib_lite_input_queue_t){0};
    queue->events = storage;
    queue->capacity = capacity;
    if (sync) queue->sync = *sync;
    queue->initialized = true;
    return RAYLIB_LITE_OK;
}

void raylib_lite_input_queue_deinit(raylib_lite_input_queue_t *queue)
{
    if (!queue) return;
    *queue = (raylib_lite_input_queue_t){0};
}

void raylib_lite_input_queue_reset(raylib_lite_input_queue_t *queue)
{
    if (!queue || !queue->initialized) return;
    queue_lock(queue);
    queue->read_index = 0;
    queue->count = 0;
    queue->dropped_events = 0;
    queue_unlock(queue);
}

raylib_lite_result_t raylib_lite_input_push(
    raylib_lite_input_queue_t *queue,
    const raylib_lite_input_event_t *event)
{
    if (!queue || !event) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!queue->initialized) return RAYLIB_LITE_INVALID_STATE;
    queue_lock(queue);
    if (queue->count == queue->capacity) {
        ++queue->dropped_events;
        queue_unlock(queue);
        return RAYLIB_LITE_BUSY;
    }
    size_t write_index = (queue->read_index + queue->count) % queue->capacity;
    queue->events[write_index] = *event;
    ++queue->count;
    queue_unlock(queue);
    return RAYLIB_LITE_OK;
}

bool raylib_lite_input_poll(raylib_lite_input_queue_t *queue,
                            raylib_lite_input_event_t *out_event)
{
    if (!queue || !out_event || !queue->initialized) return false;
    queue_lock(queue);
    if (queue->count == 0) {
        queue_unlock(queue);
        return false;
    }
    *out_event = queue->events[queue->read_index];
    queue->read_index = (queue->read_index + 1) % queue->capacity;
    --queue->count;
    queue_unlock(queue);
    return true;
}

uint32_t raylib_lite_input_dropped(raylib_lite_input_queue_t *queue)
{
    if (!queue || !queue->initialized) return 0;
    queue_lock(queue);
    uint32_t dropped = queue->dropped_events;
    queue_unlock(queue);
    return dropped;
}

size_t raylib_lite_input_count(raylib_lite_input_queue_t *queue)
{
    if (!queue || !queue->initialized) return 0;
    queue_lock(queue);
    size_t count = queue->count;
    queue_unlock(queue);
    return count;
}
