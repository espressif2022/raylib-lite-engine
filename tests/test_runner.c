// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_input.h"
#include "raylib_lite_runner.h"

typedef struct {
    uint64_t now;
    uint64_t sleeps;
} fake_clock_t;

static uint64_t fake_now(void *context)
{
    return ((fake_clock_t *)context)->now;
}

static void fake_sleep(void *context, uint64_t duration)
{
    fake_clock_t *clock = context;
    clock->now += duration;
    ++clock->sleeps;
}

static void fake_early_sleep(void *context, uint64_t duration)
{
    fake_clock_t *clock = context;
    clock->now += duration > 1 ? duration / 2 : duration;
    ++clock->sleeps;
}

typedef struct {
    fake_clock_t *clock;
    unsigned updates;
    unsigned renders;
    unsigned polls;
    unsigned close_after_renders;
    bool idle_once;
    bool stalled;
} runner_fixture_t;

static bool close_runner(void *user)
{
    runner_fixture_t *fixture = user;
    return fixture->renders >= fixture->close_after_renders;
}

static bool idle_runner(void *user)
{
    runner_fixture_t *fixture = user;
    if (!fixture->idle_once) return false;
    fixture->idle_once = false;
    return true;
}

static void poll_runner(void *user)
{
    ++((runner_fixture_t *)user)->polls;
}

static void update_runner(void *user)
{
    ++((runner_fixture_t *)user)->updates;
}

static void render_runner(void *user)
{
    runner_fixture_t *fixture = user;
    ++fixture->renders;
    if (!fixture->stalled) {
        fixture->clock->now += 500000;
        fixture->stalled = true;
    }
}

typedef struct {
    raylib_lite_input_queue_t *queue;
    int first;
    int count;
    atomic_int *completed;
} producer_args_t;

static void *produce(void *value)
{
    producer_args_t *args = value;
    for (int i = 0; i < args->count; ++i) {
        raylib_lite_input_event_t event = {
            .type = RAYLIB_LITE_INPUT_BUTTON,
            .value = args->first + i,
        };
        raylib_lite_result_t result;
        do {
            result = raylib_lite_input_push(args->queue, &event);
            if (result == RAYLIB_LITE_BUSY) sched_yield();
        } while (result == RAYLIB_LITE_BUSY);
        assert(result == RAYLIB_LITE_OK);
    }
    if (args->completed) atomic_fetch_add(args->completed, 1);
    return NULL;
}

static void mutex_lock(void *context)
{
    assert(!pthread_mutex_lock(context));
}

static void mutex_unlock(void *context)
{
    assert(!pthread_mutex_unlock(context));
}

static void test_queue(void)
{
    raylib_lite_input_event_t storage[4];
    raylib_lite_input_queue_t queue;
    assert(raylib_lite_input_queue_init(&queue, storage, 4, NULL) ==
           RAYLIB_LITE_OK);
    for (int i = 0; i < 4; ++i) {
        raylib_lite_input_event_t event = {.value = i};
        assert(raylib_lite_input_push(&queue, &event) == RAYLIB_LITE_OK);
    }
    raylib_lite_input_event_t overflow = {.value = 99};
    assert(raylib_lite_input_push(&queue, &overflow) == RAYLIB_LITE_BUSY);
    assert(raylib_lite_input_dropped(&queue) == 1);
    for (int i = 0; i < 2; ++i) {
        raylib_lite_input_event_t event;
        assert(raylib_lite_input_poll(&queue, &event));
        assert(event.value == i);
    }
    for (int i = 4; i < 6; ++i) {
        raylib_lite_input_event_t event = {.value = i};
        assert(raylib_lite_input_push(&queue, &event) == RAYLIB_LITE_OK);
    }
    for (int i = 2; i < 6; ++i) {
        raylib_lite_input_event_t event;
        assert(raylib_lite_input_poll(&queue, &event));
        assert(event.value == i);
    }
    assert(!raylib_lite_input_poll(&queue, &overflow));

    raylib_lite_input_queue_deinit(&queue);
    raylib_lite_input_sync_t invalid_sync = {.lock = mutex_lock};
    assert(raylib_lite_input_queue_init(&queue, storage, 4, &invalid_sync) ==
           RAYLIB_LITE_INVALID_ARGUMENT);

    raylib_lite_input_event_t concurrent_storage[16];
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    raylib_lite_input_sync_t sync = {
        .context = &mutex, .lock = mutex_lock, .unlock = mutex_unlock,
    };
    assert(raylib_lite_input_queue_init(&queue, concurrent_storage, 16, &sync) ==
           RAYLIB_LITE_OK);
    atomic_int completed = 0;
    producer_args_t args[2] = {
        {&queue, 0, 100, &completed}, {&queue, 100, 100, &completed},
    };
    pthread_t threads[2];
    assert(!pthread_create(&threads[0], NULL, produce, &args[0]));
    assert(!pthread_create(&threads[1], NULL, produce, &args[1]));
    bool seen[200] = {0};
    int last[2] = {-1, 99};
    size_t received = 0;
    raylib_lite_input_event_t event;
    while (atomic_load(&completed) != 2 || raylib_lite_input_count(&queue)) {
        if (!raylib_lite_input_poll(&queue, &event)) {
            sched_yield();
            continue;
        }
        assert(event.value >= 0 && event.value < 200);
        assert(!seen[event.value]);
        seen[event.value] = true;
        int producer = event.value >= 100;
        assert(event.value > last[producer]);
        last[producer] = event.value;
        ++received;
    }
    assert(!pthread_join(threads[0], NULL));
    assert(!pthread_join(threads[1], NULL));
    assert(received == 200);
    for (int i = 0; i < 200; ++i) assert(seen[i]);
    raylib_lite_input_queue_deinit(&queue);
    assert(!pthread_mutex_destroy(&mutex));
}

static void test_runner(void)
{
    fake_clock_t fake = {0};
    runner_fixture_t fixture = {
        .clock = &fake,
        .close_after_renders = 3,
        .idle_once = true,
    };
    raylib_lite_clock_t clock = {
        .context = &fake,
        .monotonic_us = fake_now,
        .sleep_for_us = fake_sleep,
    };
    raylib_lite_runner_config_t config = {
        .user = &fixture,
        .logic_hz = 30,
        .target_fps = 30,
        .should_close = close_runner,
        .idle = idle_runner,
        .poll_input = poll_runner,
        .update = update_runner,
        .render = render_runner,
    };
    raylib_lite_runner_stats_t stats;
    assert(raylib_lite_runner_run(&config, &clock, &stats) == RAYLIB_LITE_OK);
    assert(fixture.renders == 3);
    assert(stats.idle_passes == 1);
    assert(stats.logic_updates == 5); /* Immediate, capped 3 after stall, then 1. */
    assert(stats.discarded_logic_updates >= 12);
    assert(stats.discarded_render_frames >= 14);
    assert(fixture.polls == stats.scheduler_passes);
    assert(fake.sleeps >= 1);
}

static uint32_t dynamic_fps(void *user)
{
    (void)user;
    return 60;
}

static void test_early_wake_and_dynamic_fps(void)
{
    fake_clock_t fake = {0};
    runner_fixture_t fixture = {
        .clock = &fake,
        .close_after_renders = 4,
        .stalled = true,
    };
    raylib_lite_clock_t clock = {
        .context = &fake,
        .monotonic_us = fake_now,
        .sleep_for_us = fake_early_sleep,
    };
    raylib_lite_runner_config_t config = {
        .user = &fixture,
        .logic_hz = 30,
        .target_fps = 10,
        .should_close = close_runner,
        .update = update_runner,
        .render = render_runner,
        .render_fps = dynamic_fps,
    };
    raylib_lite_runner_stats_t stats;
    assert(raylib_lite_runner_run(&config, &clock, &stats) == RAYLIB_LITE_OK);
    assert(stats.rendered_frames == 4);
    assert(stats.logic_updates == 2);
    assert(fake.sleeps > 3); /* Early wakeups are rechecked, not treated as due. */

    raylib_lite_runner_config_t invalid = config;
    invalid.logic_hz = 0;
    assert(raylib_lite_runner_run(&invalid, &clock, NULL) ==
           RAYLIB_LITE_INVALID_ARGUMENT);
    clock.sleep_for_us = NULL;
    assert(raylib_lite_runner_run(&config, &clock, NULL) ==
           RAYLIB_LITE_INVALID_ARGUMENT);
}

int main(void)
{
    test_queue();
    test_runner();
    test_early_wake_and_dynamic_fps();
    puts("runner and input queue: ok");
    return 0;
}
