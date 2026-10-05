// SPDX-License-Identifier: Apache-2.0
#include "platform_esp_audio.h"

#include <stdatomic.h>
#include <stdlib.h>
#include "bsp/esp_mosaico.h"
#include "esp_codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define AUDIO_CHUNK_FRAMES 240U
#define AUDIO_DONE_BIT BIT0
#define AUDIO_WRITE_TIMEOUT_MS 20U

struct platform_esp_audio_service {
    SemaphoreHandle_t mutex;
    EventGroupHandle_t events;
    TaskHandle_t task;
    platform_esp_audio_pull_fn pull;
    void *pull_context;
    esp_codec_dev_handle_t codec;
    atomic_bool stopping;
    bool ready;
    platform_esp_audio_stats_t stats;
    raylib_lite_audio_backend_t backend;
};

static raylib_lite_result_t codec_start(
    void *context, const raylib_lite_audio_format_t *format)
{
    platform_esp_audio_service_t *service = context;
    if (!format || format->sample_rate != RAYLIB_LITE_AUDIO_SAMPLE_RATE ||
        format->channels != RAYLIB_LITE_AUDIO_CHANNELS ||
        format->format != RAYLIB_LITE_PCM_S16_NATIVE) {
        return RAYLIB_LITE_NOT_SUPPORTED;
    }
    i2s_std_config_t config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(RAYLIB_LITE_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(16, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = BSP_AUDIO_I2S_MCLK,
            .bclk = BSP_AUDIO_I2S_SCLK,
            .ws = BSP_AUDIO_I2S_LRCLK,
            .dout = BSP_AUDIO_I2S_SDOUT,
            .din = GPIO_NUM_NC,
        },
    };
    config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    if (bsp_audio_init(&config) != ESP_OK) return RAYLIB_LITE_PLATFORM_ERROR;
    service->codec = bsp_audio_codec_speaker_init();
    esp_codec_dev_sample_info_t info = {
        .sample_rate = RAYLIB_LITE_AUDIO_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
    };
    if (!service->codec || esp_codec_dev_open(service->codec, &info) != ESP_CODEC_DEV_OK) {
        service->codec = NULL;
        return RAYLIB_LITE_NOT_READY;
    }
    if (esp_codec_dev_set_out_vol(service->codec, 72) != ESP_CODEC_DEV_OK) {
        esp_codec_dev_close(service->codec);
        service->codec = NULL;
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_write(
    void *context, const int16_t *frames, size_t frame_count,
    uint32_t timeout_ms, size_t *out_written)
{
    (void)timeout_ms;
    platform_esp_audio_service_t *service = context;
    *out_written = 0;
    if (atomic_load_explicit(&service->stopping, memory_order_acquire))
        return RAYLIB_LITE_NOT_READY;
    if (!service->codec) return RAYLIB_LITE_NOT_READY;
    if (esp_codec_dev_write(service->codec, (void *)frames,
                            frame_count * sizeof(*frames)) != ESP_CODEC_DEV_OK) {
        return RAYLIB_LITE_IO_ERROR;
    }
    *out_written = frame_count;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_stop(void *context, uint32_t timeout_ms)
{
    (void)timeout_ms;
    platform_esp_audio_service_t *service = context;
    if (service->codec) {
        esp_codec_dev_close(service->codec);
        service->codec = NULL;
    }
    return RAYLIB_LITE_OK;
}

static bool service_should_stop(void *context)
{
    platform_esp_audio_service_t *service = context;
    return atomic_load_explicit(&service->stopping, memory_order_acquire);
}

static void audio_worker(void *argument)
{
    platform_esp_audio_service_t *service = argument;
    raylib_lite_audio_format_t format = RAYLIB_LITE_AUDIO_FORMAT_DEFAULT();
    if (service->backend.start(service->backend.context, &format) != RAYLIB_LITE_OK) {
        xSemaphoreTake(service->mutex, portMAX_DELAY);
        service->ready = false;
        xSemaphoreGive(service->mutex);
        xEventGroupSetBits(service->events, AUDIO_DONE_BIT);
        vTaskSuspend(NULL);
        return; /* Unreachable: stop owns deletion after observing DONE. */
    }
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    service->ready = true;
    xSemaphoreGive(service->mutex);
    int16_t output[AUDIO_CHUNK_FRAMES];
    while (!atomic_load_explicit(&service->stopping, memory_order_acquire)) {
        if (service->pull(service->pull_context, output, AUDIO_CHUNK_FRAMES) !=
            RAYLIB_LITE_OK) break;
        size_t written = 0;
        raylib_lite_result_t result = platform_audio_write_all(
            &service->backend, output, AUDIO_CHUNK_FRAMES,
            AUDIO_WRITE_TIMEOUT_MS, 2, service_should_stop, service, &written);
        if (result == RAYLIB_LITE_BUSY || result == RAYLIB_LITE_TIMEOUT) {
            xSemaphoreTake(service->mutex, portMAX_DELAY);
            ++service->stats.underruns;
            xSemaphoreGive(service->mutex);
            (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
            continue;
        }
        if (result != RAYLIB_LITE_OK) {
            if (!atomic_load_explicit(&service->stopping, memory_order_acquire)) {
                xSemaphoreTake(service->mutex, portMAX_DELAY);
                ++service->stats.write_errors;
                xSemaphoreGive(service->mutex);
            }
            break;
        }
    }
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    service->ready = false;
    xSemaphoreGive(service->mutex);
    (void)service->backend.stop(service->backend.context, 0);
    xEventGroupSetBits(service->events, AUDIO_DONE_BIT);
    vTaskSuspend(NULL);
}

raylib_lite_result_t platform_esp_audio_service_create(
    platform_esp_audio_service_t **out_service)
{
    if (!out_service) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_service = NULL;
    platform_esp_audio_service_t *service = calloc(1, sizeof(*service));
    if (!service) return RAYLIB_LITE_NO_MEMORY;
    service->mutex = xSemaphoreCreateMutex();
    service->events = xEventGroupCreate();
    if (!service->mutex || !service->events) {
        if (service->events) vEventGroupDelete(service->events);
        if (service->mutex) vSemaphoreDelete(service->mutex);
        free(service);
        return RAYLIB_LITE_NO_MEMORY;
    }
    service->backend = (raylib_lite_audio_backend_t) {
        .context = service, .start = codec_start,
        .write = codec_write, .stop = codec_stop,
    };
    atomic_init(&service->stopping, false);
    *out_service = service;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service,
    platform_esp_audio_pull_fn pull, void *pull_context)
{
    if (!service || !pull) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (service->task) return RAYLIB_LITE_INVALID_STATE;
    service->pull = pull;
    service->pull_context = pull_context;
    atomic_store_explicit(&service->stopping, false, memory_order_release);
    service->ready = false;
    xEventGroupClearBits(service->events, AUDIO_DONE_BIT);
    if (xTaskCreate(audio_worker, "game_audio", 4096, service, 6,
                    &service->task) != pdPASS) {
        service->task = NULL;
        return RAYLIB_LITE_NO_MEMORY;
    }
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    if (!service) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!service->task) return RAYLIB_LITE_OK;
    if (timeout_ms == RAYLIB_LITE_WAIT_FOREVER)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    atomic_store_explicit(&service->stopping, true, memory_order_release);
    if (!(xEventGroupGetBits(service->events) & AUDIO_DONE_BIT)) {
        xTaskNotifyGive(service->task);
    }
    TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : 0;
    if (timeout_ms && ticks == 0) ticks = 1;
    EventBits_t bits = xEventGroupWaitBits(
        service->events, AUDIO_DONE_BIT, pdFALSE, pdTRUE, ticks);
    if (!(bits & AUDIO_DONE_BIT)) return RAYLIB_LITE_TIMEOUT;
    vTaskDelete(service->task);
    service->task = NULL;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms)
{
    if (!service) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_result_t result = platform_esp_audio_service_stop(
        service, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;
    vEventGroupDelete(service->events);
    vSemaphoreDelete(service->mutex);
    free(service);
    return RAYLIB_LITE_OK;
}

bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service)
{
    if (!service) return false;
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    bool ready = service->ready;
    xSemaphoreGive(service->mutex);
    return ready;
}

void platform_esp_audio_service_get_stats(
    platform_esp_audio_service_t *service, platform_esp_audio_stats_t *out_stats)
{
    if (!service || !out_stats) return;
    xSemaphoreTake(service->mutex, portMAX_DELAY);
    *out_stats = service->stats;
    xSemaphoreGive(service->mutex);
}

void platform_esp_audio_service_lock(void *opaque)
{
    platform_esp_audio_service_t *service = opaque;
    if (service) xSemaphoreTake(service->mutex, portMAX_DELAY);
}

void platform_esp_audio_service_unlock(void *opaque)
{
    platform_esp_audio_service_t *service = opaque;
    if (service) xSemaphoreGive(service->mutex);
}
