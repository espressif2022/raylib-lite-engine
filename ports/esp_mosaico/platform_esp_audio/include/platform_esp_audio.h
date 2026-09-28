// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct platform_esp_audio_service platform_esp_audio_service_t;

typedef raylib_lite_result_t (*platform_esp_audio_pull_fn)(
    void *context, int16_t *out_frames, size_t frame_count);
typedef bool (*platform_audio_should_stop_fn)(void *context);

typedef struct {
    uint32_t write_errors;
    uint32_t underruns;
} platform_esp_audio_stats_t;

/* Pure short-write helper, also used by Host tests. max_no_progress bounds
 * repeated OK+0/BUSY/TIMEOUT results; the caller owns any delay policy. */
raylib_lite_result_t platform_audio_write_all(
    const raylib_lite_audio_backend_t *backend, const int16_t *frames,
    size_t frame_count, uint32_t timeout_ms, uint32_t max_no_progress,
    platform_audio_should_stop_fn should_stop, void *stop_context,
    size_t *out_written);

raylib_lite_result_t platform_esp_audio_service_create(
    platform_esp_audio_service_t **out_service);
raylib_lite_result_t platform_esp_audio_service_start(
    platform_esp_audio_service_t *service,
    platform_esp_audio_pull_fn pull, void *pull_context);
/* A codec write already executing in esp_codec_dev cannot be cancelled by its
 * current API. timeout bounds the join wait; TIMEOUT keeps service alive in a
 * stopping state and callers must retry before destroy. */
raylib_lite_result_t platform_esp_audio_service_stop(
    platform_esp_audio_service_t *service, uint32_t timeout_ms);
raylib_lite_result_t platform_esp_audio_service_destroy(
    platform_esp_audio_service_t *service, uint32_t timeout_ms);
bool platform_esp_audio_service_ready(platform_esp_audio_service_t *service);
void platform_esp_audio_service_get_stats(
    platform_esp_audio_service_t *service, platform_esp_audio_stats_t *out_stats);

/* These callbacks let the portable mixer serialize game-thread controls with
 * the platform worker without including FreeRTOS types in the mixer. */
void platform_esp_audio_service_lock(void *service);
void platform_esp_audio_service_unlock(void *service);

#ifdef __cplusplus
}
#endif
