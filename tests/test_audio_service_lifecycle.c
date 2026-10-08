// SPDX-License-Identifier: Apache-2.0
/* Deterministic FreeRTOS scheduler fake: exercise the production service's
 * join/backend-teardown boundary with injected startup and close failures. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_esp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct fake_event_group { EventBits_t bits; };
struct fake_semaphore { int unused; };
struct fake_task { void (*entry)(void *); void *context; bool ran; };

static TaskHandle_t current_task;
static bool defer_execution;
static bool fail_create;
static bool fail_backend_start;
static unsigned fail_backend_stop_count;
static unsigned start_calls, stop_calls, join_calls, worker_suspend_calls;
static unsigned live_events, live_mutexes;
static bool backend_live;

EventGroupHandle_t xEventGroupCreate(void)
{
    EventGroupHandle_t p = calloc(1, sizeof(*p));
    if (p) ++live_events;
    return p;
}
void vEventGroupDelete(EventGroupHandle_t p)
{ assert(p); --live_events; free(p); }
EventBits_t xEventGroupSetBits(EventGroupHandle_t p, EventBits_t bits)
{ return p->bits |= bits; }
EventBits_t xEventGroupClearBits(EventGroupHandle_t p, EventBits_t bits)
{ p->bits &= ~bits; return p->bits; }
EventBits_t xEventGroupGetBits(EventGroupHandle_t p) { return p->bits; }

EventBits_t xEventGroupWaitBits(EventGroupHandle_t p, EventBits_t bits,
    BaseType_t clear_on_exit, BaseType_t wait_all, TickType_t ticks)
{
    (void)ticks;
    if (current_task && !current_task->ran && !defer_execution) {
        current_task->ran = true;
        current_task->entry(current_task->context);
    }
    EventBits_t value = p->bits;
    bool match = wait_all ? (value & bits) == bits : (value & bits) != 0;
    if (match && clear_on_exit) p->bits &= ~bits;
    return value;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    SemaphoreHandle_t p = calloc(1, sizeof(*p));
    if (p) ++live_mutexes;
    return p;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t p, TickType_t ticks)
{ (void)ticks; assert(p); return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t p)
{ assert(p); return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t p)
{ assert(p); --live_mutexes; free(p); }

BaseType_t xTaskCreate(void (*fn)(void *), const char *name,
    uint32_t stack, void *ctx, uint32_t priority, TaskHandle_t *out)
{
    (void)name; (void)stack; (void)priority;
    if (fail_create) return pdFALSE;
    assert(!current_task);
    current_task = calloc(1, sizeof(*current_task));
    assert(current_task);
    current_task->entry = fn;
    current_task->context = ctx;
    *out = current_task;
    return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t task) { assert(task == current_task); }
uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t ticks)
{ (void)clear_on_exit; (void)ticks; return 0; }
void vTaskSuspend(TaskHandle_t task)
{ assert(!task); ++worker_suspend_calls; }
void vTaskDelete(TaskHandle_t task)
{
    assert(task && task == current_task && task->ran);
    free(task);
    current_task = NULL;
    ++join_calls;
}

static raylib_lite_result_t fake_start(void *ctx,
    const raylib_lite_audio_format_t *format)
{
    (void)ctx;
    assert(format && format->sample_rate == 24000);
    assert(current_task);
    ++start_calls;
    backend_live = true; /* Includes partial start before reporting error. */
    return fail_backend_start ? RAYLIB_LITE_PLATFORM_ERROR : RAYLIB_LITE_OK;
}

static raylib_lite_result_t fake_write(void *ctx, const int16_t *frames,
    size_t count, uint32_t timeout_ms, size_t *written)
{
    (void)ctx; (void)frames; (void)timeout_ms;
    assert(current_task && backend_live);
    *written = count;
    return RAYLIB_LITE_OK;
}
static raylib_lite_result_t fake_stop(void *ctx, uint32_t timeout_ms)
{
    (void)ctx; (void)timeout_ms;
    /* This is the invariant Copilot found missing: backend cleanup is never
     * allowed while the Worker task can access the codec. */
    assert(!current_task);
    ++stop_calls;
    if (fail_backend_stop_count) {
        --fail_backend_stop_count;
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    backend_live = false;
    return RAYLIB_LITE_OK;
}
raylib_lite_audio_backend_t platform_esp_audio_board_backend(void)
{
    return (raylib_lite_audio_backend_t) {
        .start = fake_start, .write = fake_write, .stop = fake_stop,
    };
}
static raylib_lite_result_t fake_pull(void *ctx, int16_t *dst, size_t frames)
{ (void)ctx; (void)dst; (void)frames; return RAYLIB_LITE_IO_ERROR; }

static void reset(void)
{
    assert(!current_task && !backend_live && !live_events && !live_mutexes);
    defer_execution = fail_create = fail_backend_start = false;
    start_calls = stop_calls = join_calls = worker_suspend_calls = 0;
    fail_backend_stop_count = 0;
}
static void all_released(void)
{ assert(!current_task && !backend_live && !live_events && !live_mutexes); }

int main(void)
{
    platform_esp_audio_service_t *service;
    reset();

    /* Partial backend.start failure still requires teardown. A failed stop
     * must not destroy the service; a later stop retries the same backend. */
    fail_backend_start = true;
    fail_backend_stop_count = 2;
    assert(platform_esp_audio_service_create(&service) == RAYLIB_LITE_OK);
    assert(platform_esp_audio_service_start(service, fake_pull, NULL) ==
           RAYLIB_LITE_NOT_READY);
    assert(start_calls == 1 && stop_calls == 0 && backend_live);
    assert(platform_esp_audio_service_stop(service, 10) ==
           RAYLIB_LITE_PLATFORM_ERROR);
    assert(join_calls == 1 && stop_calls == 1 && backend_live);
    assert(platform_esp_audio_service_destroy(service, 10) ==
           RAYLIB_LITE_PLATFORM_ERROR);
    assert(stop_calls == 2 && backend_live && live_events == 1);
    assert(platform_esp_audio_service_stop(service, 10) == RAYLIB_LITE_OK);
    assert(stop_calls == 3 && !backend_live);
    assert(platform_esp_audio_service_destroy(service, 10) == RAYLIB_LITE_OK);
    all_released();

    /* Timeout while a worker hasn't finished is not permission to call stop.
     * Simulate the Worker being delayed beyond the startup and join deadline. */
    reset();
    defer_execution = true;
    assert(platform_esp_audio_service_create(&service) == RAYLIB_LITE_OK);
    assert(platform_esp_audio_service_start(service, fake_pull, NULL) ==
           RAYLIB_LITE_TIMEOUT);
    assert(start_calls == 0);
    assert(platform_esp_audio_service_stop(service, 0) == RAYLIB_LITE_TIMEOUT);
    assert(stop_calls == 0 && join_calls == 0 && live_events == 1);
    defer_execution = false;
    assert(platform_esp_audio_service_stop(service, 30) == RAYLIB_LITE_OK);
    assert(start_calls == 1 && stop_calls == 1 && join_calls == 1);
    assert(platform_esp_audio_service_destroy(service, 30) == RAYLIB_LITE_OK);
    all_released();

    /* Task creation failure must not require backend.stop (never started). */
    reset();
    fail_create = true;
    assert(platform_esp_audio_service_create(&service) == RAYLIB_LITE_OK);
    assert(platform_esp_audio_service_start(service, fake_pull, NULL) ==
           RAYLIB_LITE_NO_MEMORY);
    assert(platform_esp_audio_service_destroy(service, 0) == RAYLIB_LITE_OK);
    assert(stop_calls == 0 && join_calls == 0);
    all_released();

    /* A worker that exits after READY is still joined before codec release. */
    reset();
    assert(platform_esp_audio_service_create(&service) == RAYLIB_LITE_OK);
    assert(platform_esp_audio_service_start(service, fake_pull, NULL) ==
           RAYLIB_LITE_NOT_READY);
    assert(start_calls == 1 && stop_calls == 0);
    assert(platform_esp_audio_service_stop(service, 30) == RAYLIB_LITE_OK);
    assert(join_calls == 1 && stop_calls == 1);
    assert(platform_esp_audio_service_destroy(service, 30) == RAYLIB_LITE_OK);
    all_released();

    puts("audio service lifecycle: ok");
    return 0;
}
