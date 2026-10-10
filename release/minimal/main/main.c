// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_runner.h"
#include "raylib_lite_renderer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static unsigned updates;
static uint16_t pixels[32 * 32];
static uint64_t now(void *context) { (void)context; return esp_timer_get_time(); }
static void sleep_us(void *context, uint64_t duration)
{
    (void)context;
    if (duration) vTaskDelay(pdMS_TO_TICKS((duration + 999) / 1000) + 1);
}
static bool done(void *context) { (void)context; return updates >= 30; }
static void update(void *context) { (void)context; ++updates; }
static void render(void *context)
{
    (void)context;
    // An offscreen CPU framebuffer: no display, BSP, Iris or Recovery required.
    raylib_lite_renderer_set_target(pixels, 32, 32, 32);
    for (unsigned i = 0; i < 32 * 32; ++i) pixels[i] = (uint16_t)updates;
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);
}
void app_main(void)
{
    const raylib_lite_clock_t clock = {.monotonic_us = now, .sleep_for_us = sleep_us};
    const raylib_lite_runner_config_t config = {
        .logic_hz = 30, .target_fps = 30, .should_close = done,
        .update = update, .render = render,
    };
    raylib_lite_runner_stats_t stats = {0};
    raylib_lite_result_t result = raylib_lite_runner_run(&config, &clock, &stats);
    ESP_LOGI("raylib_lite_minimal", "result=%d updates=%llu frames=%llu", result,
             (unsigned long long)stats.logic_updates, (unsigned long long)stats.rendered_frames);
}
