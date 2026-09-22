// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_app.h"

#include <string.h>
#include "bsp/esp_mosaico.h"
#include "esp_check.h"
#include "esp_display_present_config.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "mosaico_game_action.h"
#include "mosaico_game_debug.h"
#include "mosaico_game_input.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const mosaico_game_app_config_t *s_config;
static esp_lcd_touch_handle_t s_touch;
static esp_gsp_handle_t s_gsp;

static void imu_task(void *ctx)
{
    (void)ctx;
    const TickType_t period = pdMS_TO_TICKS(
        s_config->imu_sample_ms ? s_config->imu_sample_ms : 20);
    TickType_t wake = xTaskGetTickCount();
    while (true) {
        float x = 0, y = 0, z = 0;
        if (bsp_imu_get_accel(&x, &y, &z) == ESP_OK)
            (void)mosaico_game_input_imu(x, y, z, esp_timer_get_time());
        xTaskDelayUntil(&wake, period);
    }
}

static void touch_task(void *ctx)
{
    (void)ctx;
    const uint8_t max_points = s_config->touch_points ? s_config->touch_points : 1;
    mosaico_input_contact_t previous[MOSAICO_CONTACT_CAPACITY] = {0};
    bool was_pressed = false;
    while (true) {
        esp_lcd_touch_point_data_t points[MOSAICO_CONTACT_CAPACITY] = {0};
        uint8_t count = 0;
        uint64_t now = esp_timer_get_time();
        if (esp_lcd_touch_read_data(s_touch) != ESP_OK ||
            esp_lcd_touch_get_data(s_touch, points, &count, max_points) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(12));
            continue;
        }
        if (count > max_points) count = max_points;
        if (max_points <= 1) {
            static int last_x = 0, last_y = 0;
            bool pressed = count > 0;
            if (pressed) {
                last_x = points[0].x;
                last_y = points[0].y;
            }
            if (pressed || was_pressed)
                (void)mosaico_game_input_pointer(last_x, last_y, pressed, now);
            was_pressed = pressed;
        } else {
            for (uint8_t i = 0; i < count; ++i) {
                if (points[i].x >= MOSAICO_GAME_WIDTH || points[i].y >= MOSAICO_GAME_HEIGHT)
                    continue;
                (void)mosaico_game_input_touch(points[i].track_id, points[i].x,
                                               points[i].y, true, now);
            }
            for (size_t old = 0; old < MOSAICO_CONTACT_CAPACITY; ++old) {
                if (!previous[old].active) continue;
                bool still = false;
                for (uint8_t i = 0; i < count; ++i)
                    if (points[i].track_id == previous[old].track_id) still = true;
                if (!still)
                    (void)mosaico_game_input_touch(previous[old].track_id,
                        previous[old].x, previous[old].y, false, now);
            }
            memset(previous, 0, sizeof(previous));
            for (uint8_t i = 0; i < count && i < MOSAICO_CONTACT_CAPACITY; ++i)
                previous[i] = (mosaico_input_contact_t){
                    .track_id = points[i].track_id, .x = points[i].x,
                    .y = points[i].y, .active = true};
        }
        vTaskDelay(pdMS_TO_TICKS(12));
    }
}

static esp_err_t start_display(void)
{
    const char *tag = s_config->tag ? s_config->tag : "mosaico_game_app";
    bsp_display_config_t cfg = BSP_DISPLAY_DEFAULT_CONFIG();
    cfg.enable_touch = false;
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(bsp_display_new(&cfg, &panel), tag, "display");
    ESP_RETURN_ON_ERROR(bsp_touch_new(BSP_DISPLAY_ROTATE_0, &s_touch), tag, "touch");
    if (!s_config->gsp_bundle) return ESP_ERR_INVALID_ARG;
    esp_gsp_config_t app = s_config->gsp_bundle();
    esp_gsp_esp_lcd_config_t host = ESP_GSP_ESP_LCD_CONFIG_INIT();
    host.perf_log = true;
    host.display = (esp_display_present_target_config_t){
        .hw = {.panel = panel, .io = bsp_display_get_panel_io(),
            .panel_type = ESP_DISPLAY_PRESENT_PANEL_IO,
            .input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
            .rotation = ESP_DISPLAY_PRESENT_ROTATE_0, .swap_bytes = true,
            .te_enabled = false,
            .te_sync = ESP_DISPLAY_PRESENT_TE_SYNC_DISABLED()},
        /* Games redraw continuously.  Submit directly to panel GRAM instead
         * of holding complete frames for the TE-synchronised scheduler. */
        .fb = {.mode = ESP_DISPLAY_PRESENT_MODE_NONE}};
    if (s_config->drawbuf_lines || s_config->te_compose_buffers) {
        host.display.drawbuf.lines = s_config->drawbuf_lines;
        host.display.drawbuf.in_psram = true;
        host.display.drawbuf.te_compose_buffers = s_config->te_compose_buffers;
    }
    return esp_gsp_esp_lcd_start(&app, &host, &s_gsp);
}

static void run_game_loop(const mosaico_game_app_config_t *config,int target_fps,int logic_hz)
{
    const char *tag=config->tag?config->tag:"mosaico_game_app";
    ESP_LOGI(tag,"game loop core=%d logic=%d target=%d",xPortGetCoreID(),logic_hz,target_fps);
    int64_t previous_us = esp_timer_get_time();
    /* Separate fixed gameplay ticks from presentation. Credits use
     * microseconds * Hz and start with one immediate update/frame. */
    int64_t logic_credit = 1000000;
    int64_t render_credit = 1000000;
    uint32_t frames = 0;
    uint32_t stats_interval = config->stats_interval ? config->stats_interval : 100;
    while (!WindowShouldClose()) {
        int64_t input_started = esp_timer_get_time();
        mosaico_device_event_t event;
        while (MosaicoGamePollDeviceEvent(&event)) {
            if (event.type == MOSAICO_DEVICE_EVENT_POINTER)
                MosaicoFastInjectPointer(0, event.x, event.y, event.pressed);
            else if (event.type == MOSAICO_DEVICE_EVENT_TOUCH)
                MosaicoFastInjectPointer(event.value, event.x, event.y, event.pressed);
            else if (event.type == MOSAICO_DEVICE_EVENT_IMU)
                MosaicoFastInjectImu(event.x / 1000.0f, event.y / 1000.0f,
                                     event.value / 1000.0f);
            mosaico_action_apply_event(&event);
            if (config->on_event) config->on_event(&event);
        }
        uint32_t input_us = (uint32_t)(esp_timer_get_time() - input_started);
        if (config->idle && config->idle()) {
            logic_credit = 1000000;
            render_credit = 1000000;
            vTaskDelay(1);
            previous_us = esp_timer_get_time();
            continue;
        }
        int64_t now_us = esp_timer_get_time();
        int64_t elapsed_us = now_us - previous_us;
        logic_credit += elapsed_us * logic_hz;
        render_credit += elapsed_us * target_fps;
        previous_us = now_us;
        /* Bound catch-up to three updates: avoid a spiral after long stalls.
         * Preserve the fractional phase when discarding excessive backlog. */
        if (logic_credit >= 4000000)
            logic_credit = 3000000 + logic_credit % 1000000;
        /* Rendering never catches up old frames. Keep only the newest due frame. */
        if (render_credit >= 2000000)
            render_credit = 1000000 + render_credit % 1000000;
        if (logic_credit < 1000000 && render_credit < 1000000) {
            vTaskDelay(1);
            continue;
        }
        uint32_t update_us = 0;
        int64_t started;
        while (logic_credit >= 1000000) {
            mosaico_action_begin_frame();
            started = esp_timer_get_time();
            if (config->on_update) config->on_update();
            uint32_t step_us = (uint32_t)(esp_timer_get_time() - started);
            update_us += step_us;
            MosaicoGameRecordLogic(input_us, step_us);
            input_us = 0;
            MosaicoFastConsumeInputEdges();
            logic_credit -= 1000000;
        }
        if (render_credit < 1000000) continue;
        started = esp_timer_get_time();
        config->on_render();
        MosaicoGameRecordTiming(update_us, (uint32_t)(esp_timer_get_time() - started));
        render_credit -= 1000000;
        if (++frames % stats_interval == 0) {
            mosaico_game_debug_log(tag);
            if (config->on_stats) config->on_stats();
        }
        vTaskDelay(1); /* Yield to system tasks even while render is overloaded. */
    }
}

#if CONFIG_MOSAICO_GAME_LOOP_CPU1
typedef struct {
    const mosaico_game_app_config_t *config;
    int target_fps,logic_hz;
    SemaphoreHandle_t done;
} game_loop_args_t;

static void game_loop_task(void *value)
{
    const game_loop_args_t *args=value;
    SemaphoreHandle_t done=args->done;
    run_game_loop(args->config,args->target_fps,args->logic_hz);
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}

static esp_err_t run_game_loop_cpu1(const mosaico_game_app_config_t *config,
                                   int target_fps,int logic_hz)
{
    game_loop_args_t args={config,target_fps,logic_hz,xSemaphoreCreateBinary()};
    if(!args.done)return ESP_ERR_NO_MEM;
    /* Startup and its first flushed frame finish before this handoff. Keep
     * the caller blocked so configuration/arguments retain their lifetime. */
    BaseType_t created=xTaskCreatePinnedToCore(game_loop_task,"mosaico_game",
        CONFIG_ESP_MAIN_TASK_STACK_SIZE,&args,uxTaskPriorityGet(NULL),NULL,1);
    if(created!=pdPASS){vSemaphoreDelete(args.done);return ESP_ERR_NO_MEM;}
    xSemaphoreTake(args.done,portMAX_DELAY);
    vSemaphoreDelete(args.done);
    return ESP_OK;
}
#endif

esp_err_t mosaico_game_app_run(const mosaico_game_app_config_t *config)
{
    if (!config || !config->gsp_bundle || !config->on_render)
        return ESP_ERR_INVALID_ARG;
    s_config = config;
    const char *tag = config->tag ? config->tag : "mosaico_game_app";
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(bsp_power_init());
    ESP_ERROR_CHECK(bsp_power_set_vcc_3v3(true));
    mosaico_game_config_t game_config = MOSAICO_GAME_CONFIG_DEFAULT();
    if (config->target_fps > 0) game_config.target_fps = config->target_fps;
    int logic_hz = config->logic_hz > 0 ? config->logic_hz : game_config.target_fps;
    ESP_ERROR_CHECK(MosaicoGameInit(&game_config));
    mosaico_action_reset();
    if (config->before_display) ESP_ERROR_CHECK(config->before_display());
    ESP_ERROR_CHECK(start_display());
    if (config->enable_imu) {
        bsp_imu_config_t imu = BSP_IMU_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(bsp_imu_init());
        ESP_ERROR_CHECK(bsp_imu_start(&imu));
        ESP_ERROR_CHECK(xTaskCreate(imu_task, "game_imu", 4096, NULL, 5, NULL) == pdPASS
                        ? ESP_OK : ESP_ERR_NO_MEM);
    }
    ESP_ERROR_CHECK(mosaico_raylib_port_init(s_gsp, config->canvas_bind));
    if (config->register_mirror) ESP_ERROR_CHECK(config->register_mirror());
    InitWindow(MOSAICO_GAME_WIDTH, MOSAICO_GAME_HEIGHT,
               config->window_title ? config->window_title : "Mosaico");
    SetTargetFPS(game_config.target_fps);
    if (config->on_start) ESP_ERROR_CHECK(config->on_start());
    config->on_render();
    ESP_ERROR_CHECK(esp_gsp_flush(s_gsp, 3000));
    ESP_LOGI(tag, "first frame presented");
    if (config->after_healthy) ESP_ERROR_CHECK(config->after_healthy());
    ESP_ERROR_CHECK(xTaskCreate(touch_task, "game_touch", 4096, NULL, 5, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
#if CONFIG_MOSAICO_GAME_LOOP_CPU1
    return run_game_loop_cpu1(config,game_config.target_fps,logic_hz);
#else
    run_game_loop(config,game_config.target_fps,logic_hz);
#endif
    return ESP_OK;
}
