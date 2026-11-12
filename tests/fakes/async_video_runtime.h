// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
struct fake_semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    int count;
};
static SemaphoreHandle_t new_semaphore(int count)
{
    SemaphoreHandle_t s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->changed, NULL);
    s->count = count;
    return s;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return new_semaphore(1); }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return new_semaphore(0); }
int xSemaphoreTake(SemaphoreHandle_t s, unsigned wait)
{
    pthread_mutex_lock(&s->mutex);
    if (wait == portMAX_DELAY) {
        while (!s->count) pthread_cond_wait(&s->changed, &s->mutex);
    } else if (!s->count && wait) {
        struct timespec until;
        timespec_get(&until, TIME_UTC);
        until.tv_sec += wait / 1000;
        until.tv_nsec += (long)(wait % 1000) * 1000000L;
        if (until.tv_nsec >= 1000000000L) {
            ++until.tv_sec;
            until.tv_nsec -= 1000000000L;
        }
        while (!s->count && pthread_cond_timedwait(&s->changed, &s->mutex,
                                                   &until) == 0) {}
    }
    int taken = s->count > 0;
    if (taken) --s->count;
    pthread_mutex_unlock(&s->mutex);
    return taken ? pdTRUE : pdFALSE;
}
int xSemaphoreGive(SemaphoreHandle_t s)
{
    pthread_mutex_lock(&s->mutex);
    s->count = 1;
    pthread_cond_signal(&s->changed);
    pthread_mutex_unlock(&s->mutex);
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t s)
{
    pthread_mutex_destroy(&s->mutex);
    pthread_cond_destroy(&s->changed);
    free(s);
}
typedef struct { void (*fn)(void *); void *arg; } task_start_t;
static void *run_task(void *arg)
{
    task_start_t start = *(task_start_t *)arg;
    free(arg);
    start.fn(start.arg);
    return NULL;
}
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name,
                            unsigned stack, void *arg, unsigned priority,
                            TaskHandle_t *handle, int core)
{
    (void)name; (void)stack; (void)priority; (void)core;
    task_start_t *start = malloc(sizeof(*start));
    if (!start) return pdFALSE;
    *start = (task_start_t){fn, arg};
    pthread_t thread;
    if (pthread_create(&thread, NULL, run_task, start) != 0) {
        free(start);
        return pdFALSE;
    }
    pthread_detach(thread);
    *handle = (TaskHandle_t)1;
    return pdPASS;
}
void vTaskDelete(TaskHandle_t task) { (void)task; pthread_exit(NULL); }


int64_t esp_timer_get_time(void) { return 0; }
