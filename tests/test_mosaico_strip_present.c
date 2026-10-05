#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mosaico_strip_present.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

struct fake_semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    int count;
};
typedef struct {
    unsigned width, height, strip_rows;
    uint16_t assembled[64];
    unsigned y_next, acquires, submits, cancels, commits;
    int fail_submit;
    unsigned coverage;
    pthread_mutex_t gate;
    pthread_cond_t entered;
    int block_submit;
    int in_submit;
} fake_presenter_t;

int64_t esp_timer_get_time(void) { return 0; }

void *heap_caps_malloc(size_t n, unsigned caps) { (void)caps; return malloc(n); }
void heap_caps_free(void *p) { free(p); }
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

static esp_err_t resolve(void *ctx, uint32_t id, size_t row_bytes,
                         size_t remaining, size_t *rows)
{
    (void)ctx; (void)id;
    size_t requested = row_bytes ? 1 : 0;
    (void)requested;
    *rows = remaining < 2 ? remaining : 2;
    return ESP_OK;
}
esp_err_t esp_display_presenter_begin_next_frame(esp_display_presenter_t *p,
    const void *areas, void *cb, size_t cbn, size_t *area_count, bool *full)
{
    (void)areas; (void)cb; (void)cbn; (void)area_count; (void)full;
    ((fake_presenter_t *)p)->y_next = 0;
    return ESP_OK;
}
esp_err_t esp_display_presenter_acquire_buffer(esp_display_presenter_t *p,
                                                esp_display_presenter_buffer_t *b)
{
    fake_presenter_t *f = (fake_presenter_t *)p;
    static uint16_t storage[64];
    ++f->acquires;
    *b = (esp_display_presenter_buffer_t){
        .surface = {.pixels = storage},
        .capacity_bytes = (size_t)f->width * f->strip_rows * 2,
        .lease_id = f->acquires,
        .resolve_rows = resolve,
        .resolve_rows_ctx = f,
    };
    return ESP_OK;
}
esp_err_t esp_display_presenter_submit_buffer(esp_display_presenter_t *p,
    esp_display_presenter_buffer_t *b, const esp_display_present_area_t *area,
    size_t stride)
{
    fake_presenter_t *f = (fake_presenter_t *)p;
    pthread_mutex_lock(&f->gate);
    if (f->block_submit) {
        f->in_submit = 1;
        pthread_cond_signal(&f->entered);
        while (f->block_submit) pthread_cond_wait(&f->entered, &f->gate);
    }
    pthread_mutex_unlock(&f->gate);
    if (f->fail_submit) return ESP_FAIL;
    assert(area->x1 == 0 && area->x2 == (int)f->width - 1);
    assert(area->y1 == (int)f->y_next && area->y2 >= area->y1);
    assert(stride == f->width * 2);
    unsigned rows = (unsigned)(area->y2 - area->y1 + 1);
    memcpy(f->assembled + area->y1 * f->width, b->surface.pixels,
           rows * stride);
    f->y_next += rows;
    ++f->submits;
    return ESP_OK;
}
void esp_display_presenter_cancel_frame(esp_display_presenter_t *p)
{ ++((fake_presenter_t *)p)->cancels; }
esp_err_t esp_display_presenter_commit_frame(esp_display_presenter_t *p,
    const esp_display_presenter_submit_t *done)
{
    fake_presenter_t *f = (fake_presenter_t *)p;
    ++f->commits; f->coverage = done->coverage;
    assert(f->y_next == f->height);
    return ESP_OK;
}
esp_err_t esp_display_presenter_quiesce(esp_display_presenter_t *p, uint32_t ms)
{ (void)p; (void)ms; return ESP_OK; }

static void fill_frame(raylib_lite_frame_t *frame, uint16_t base)
{
    for (unsigned y = 0; y < frame->height; ++y)
        for (unsigned x = 0; x < frame->width; ++x)
            frame->pixels[y * frame->stride_pixels + x] = (uint16_t)(base + y * 16 + x);
}

int main(void)
{
    fake_presenter_t fake = {.width = 4, .height = 5, .strip_rows = 2};
    pthread_mutex_init(&fake.gate, NULL);
    pthread_cond_init(&fake.entered, NULL);
    fake.block_submit = 1;
    void *video = NULL;
    assert(mosaico_strip_present_open((esp_display_presenter_t *)&fake, 4, 5,
                                      &video) == RAYLIB_LITE_OK);
    raylib_lite_video_backend_t b = mosaico_strip_present_backend(video);
    raylib_lite_video_info_t info;
    assert(b.get_info(b.context, &info) == RAYLIB_LITE_OK);
    assert(info.width == 4 && info.height == 5 && info.stride_pixels == 4);

    uint16_t snapshot[20];
    assert(b.copy_latest(b.context, snapshot, 20) == RAYLIB_LITE_NOT_READY);
    raylib_lite_frame_t frame;
    assert(b.acquire(b.context, &frame) == RAYLIB_LITE_OK);
    fill_frame(&frame, 0x100);
    assert(b.present(b.context, &frame) == RAYLIB_LITE_OK);
    assert(frame.pixels == NULL);
    pthread_mutex_lock(&fake.gate);
    while (!fake.in_submit) pthread_cond_wait(&fake.entered, &fake.gate);
    pthread_mutex_unlock(&fake.gate);
    assert(fake.commits == 0);
    /* The renderer can acquire and draw while the panel worker is stalled. */
    assert(b.acquire(b.context, &frame) == RAYLIB_LITE_OK);
    fill_frame(&frame, 0x500);
    assert(b.copy_latest(b.context, snapshot, 20) == RAYLIB_LITE_NOT_READY);
    pthread_mutex_lock(&fake.gate);
    fake.block_submit = 0;
    pthread_cond_signal(&fake.entered);
    pthread_mutex_unlock(&fake.gate);
    assert(b.present(b.context, &frame) == RAYLIB_LITE_OK);
    assert(b.flush(b.context, 1000) == RAYLIB_LITE_OK);
    assert(fake.submits == 6 && fake.commits == 2);
    assert(fake.coverage == ESP_DISPLAY_PRESENT_COVERAGE_FULL);
    for (unsigned y = 0; y < 5; ++y)
        for (unsigned x = 0; x < 4; ++x)
            assert(fake.assembled[y * 4 + x] == 0x500 + y * 16 + x);

    /* A later renderer write must not mutate the accepted-frame snapshot. */
    assert(b.copy_latest(b.context, snapshot, 20) == RAYLIB_LITE_OK);
    assert(b.acquire(b.context, &frame) == RAYLIB_LITE_OK);
    fill_frame(&frame, 0x900);
    assert(b.copy_latest(b.context, snapshot, 20) == RAYLIB_LITE_OK);
    for (unsigned y = 0; y < 5; ++y)
        for (unsigned x = 0; x < 4; ++x)
            assert(snapshot[y * 4 + x] == 0x500 + y * 16 + x);

    /* A failed stripe submit cancels and does not replace latest. */
    fake.fail_submit = 1;
    unsigned commits_before = fake.commits, cancels_before = fake.cancels;
    assert(b.present(b.context, &frame) == RAYLIB_LITE_OK);
    assert(b.flush(b.context, 1000) == RAYLIB_LITE_PLATFORM_ERROR);
    assert(frame.pixels == NULL && fake.commits == commits_before);
    assert(fake.cancels == cancels_before + 1);
    fake.fail_submit = 0;
    assert(b.acquire(b.context, &frame) == RAYLIB_LITE_OK);
    assert(b.copy_latest(b.context, snapshot, 20) == RAYLIB_LITE_OK);
    assert(snapshot[0] == 0x500);
    b.discard(b.context, &frame);

    /* A timed-out teardown keeps the worker and frame storage retryable. */
    pthread_mutex_lock(&fake.gate);
    fake.block_submit = 1;
    fake.in_submit = 0;
    pthread_mutex_unlock(&fake.gate);
    assert(b.acquire(b.context, &frame) == RAYLIB_LITE_OK);
    fill_frame(&frame, 0xa00);
    assert(b.present(b.context, &frame) == RAYLIB_LITE_OK);
    pthread_mutex_lock(&fake.gate);
    while (!fake.in_submit) pthread_cond_wait(&fake.entered, &fake.gate);
    pthread_mutex_unlock(&fake.gate);
    assert(mosaico_strip_present_close(video, 10) == RAYLIB_LITE_TIMEOUT);
    pthread_mutex_lock(&fake.gate);
    fake.block_submit = 0;
    pthread_cond_signal(&fake.entered);
    pthread_mutex_unlock(&fake.gate);
    assert(mosaico_strip_present_close(video, 1000) == RAYLIB_LITE_OK);
    pthread_cond_destroy(&fake.entered);
    pthread_mutex_destroy(&fake.gate);
    puts("mosaico strip present: ok");
    return 0;
}
