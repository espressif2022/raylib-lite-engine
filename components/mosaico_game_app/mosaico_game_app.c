// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_app.h"

#include "mosaico_game.h"
#include "mosaico_game_action.h"
#include "mosaico_game_debug.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"
#include "raylib.h"
#include "raylib_lite_runner.h"

typedef struct {
    const raylib_lite_game_app_t *app;
    uint32_t rendered_frames;
    uint32_t update_us;
    raylib_lite_result_t terminal_result;
} app_runtime_t;

static raylib_lite_result_t last_frame_result(void)
{
    raylib_lite_result_t acquire = MosaicoFastGetLastAcquireResult();
    if (acquire != RAYLIB_LITE_OK) return acquire;
    return MosaicoFastGetLastPresentResult();
}

static bool frame_result_is_fatal(raylib_lite_result_t result)
{
    /* Runtime display/back-pressure failures drop a frame; only
     * contract/configuration failures terminate the application. */
    return result == RAYLIB_LITE_INVALID_ARGUMENT ||
        result == RAYLIB_LITE_INVALID_STATE ||
        result == RAYLIB_LITE_NOT_SUPPORTED ||
        result == RAYLIB_LITE_NO_MEMORY;
}

static mosaico_device_event_type_t legacy_event_type(
    raylib_lite_input_type_t type)
{
    switch (type) {
    case RAYLIB_LITE_INPUT_POINTER: return MOSAICO_DEVICE_EVENT_POINTER;
    case RAYLIB_LITE_INPUT_TOUCH: return MOSAICO_DEVICE_EVENT_TOUCH;
    case RAYLIB_LITE_INPUT_BUTTON: return MOSAICO_DEVICE_EVENT_BUTTON;
    case RAYLIB_LITE_INPUT_JOYSTICK: return MOSAICO_DEVICE_EVENT_JOYSTICK;
    case RAYLIB_LITE_INPUT_IMU: return MOSAICO_DEVICE_EVENT_IMU;
    case RAYLIB_LITE_INPUT_ATTACHED: return MOSAICO_DEVICE_EVENT_ATTACHED;
    case RAYLIB_LITE_INPUT_DETACHED: return MOSAICO_DEVICE_EVENT_DETACHED;
    default: return MOSAICO_DEVICE_EVENT_NONE;
    }
}

static void poll_input(void *context)
{
    app_runtime_t *runtime = context;
    raylib_lite_input_event_t input;
    while (runtime->app->input &&
            raylib_lite_input_poll(runtime->app->input, &input)) {
        mosaico_device_event_t event = {
            .type = legacy_event_type(input.type),
            .x = input.x,
            .y = input.y,
            .value = input.value,
            .pressed = input.pressed,
            .timestamp_us = input.timestamp_us,
        };
        if (event.type == MOSAICO_DEVICE_EVENT_POINTER)
            MosaicoFastInjectPointer(0, event.x, event.y, event.pressed);
        else if (event.type == MOSAICO_DEVICE_EVENT_TOUCH)
            MosaicoFastInjectPointer(event.value, event.x, event.y, event.pressed);
        else if (event.type == MOSAICO_DEVICE_EVENT_IMU)
            MosaicoFastInjectImu(event.x / 1000.0f, event.y / 1000.0f,
                                 event.value / 1000.0f);
        mosaico_action_apply_event(&event);
        if (runtime->app->on_event)
            runtime->app->on_event(runtime->app->user, &input);
    }
    /* Compatibility bridge: callers using the legacy MosaicoGamePostDeviceEvent
     * producer must continue to reach games during the migration. */
    mosaico_device_event_t legacy;
    while (MosaicoGamePollDeviceEvent(&legacy)) {
        if (legacy.type == MOSAICO_DEVICE_EVENT_POINTER)
            MosaicoFastInjectPointer(0, legacy.x, legacy.y, legacy.pressed);
        else if (legacy.type == MOSAICO_DEVICE_EVENT_TOUCH)
            MosaicoFastInjectPointer(legacy.value, legacy.x, legacy.y,
                                     legacy.pressed);
        else if (legacy.type == MOSAICO_DEVICE_EVENT_IMU)
            MosaicoFastInjectImu(legacy.x / 1000.0f, legacy.y / 1000.0f,
                                 legacy.value / 1000.0f);
        mosaico_action_apply_event(&legacy);
        if (runtime->app->on_event) {
            input = (raylib_lite_input_event_t) {
                .type = (raylib_lite_input_type_t)legacy.type,
                .x = legacy.x, .y = legacy.y, .value = legacy.value,
                .pressed = legacy.pressed,
                .timestamp_us = legacy.timestamp_us,
            };
            runtime->app->on_event(runtime->app->user, &input);
        }
    }
}

static bool should_close(void *context)
{
    app_runtime_t *runtime = context;
    return runtime->terminal_result != RAYLIB_LITE_OK || WindowShouldClose();
}

static bool idle(void *context)
{
    app_runtime_t *runtime = context;
    return runtime->app->idle && runtime->app->idle(runtime->app->user);
}

static void update(void *context)
{
    app_runtime_t *runtime = context;
    uint64_t started = runtime->app->platform.clock.monotonic_us(
        runtime->app->platform.clock.context);
    mosaico_action_begin_frame();
    if (runtime->app->on_update)
        runtime->app->on_update(runtime->app->user);
    uint32_t elapsed = (uint32_t)(
        runtime->app->platform.clock.monotonic_us(
            runtime->app->platform.clock.context) - started);
    runtime->update_us += elapsed;
    MosaicoGameRecordLogic(0, elapsed);
    MosaicoFastConsumeInputEdges();
}

static void render(void *context)
{
    app_runtime_t *runtime = context;
    uint64_t started = runtime->app->platform.clock.monotonic_us(
        runtime->app->platform.clock.context);
    runtime->app->on_render(runtime->app->user);
    raylib_lite_result_t frame_result = last_frame_result();
    /* Strip submission failures are dropped frames, as in the proven legacy
     * loop. They must not unwind the launcher and trigger app_main's reset. */
    if (frame_result_is_fatal(frame_result))
        runtime->terminal_result = frame_result;
    uint32_t elapsed = (uint32_t)(
        runtime->app->platform.clock.monotonic_us(
            runtime->app->platform.clock.context) - started);
    MosaicoGameRecordTiming(runtime->update_us, elapsed);
    runtime->update_us = 0;
    uint32_t interval = runtime->app->stats_interval
        ? runtime->app->stats_interval : 100;
    if (++runtime->rendered_frames % interval == 0) {
        mosaico_game_debug_log(runtime->app->tag
            ? runtime->app->tag : "raylib_lite");
        if (runtime->app->on_stats)
            runtime->app->on_stats(runtime->app->user);
    }
}

static uint32_t render_fps(void *context)
{
    app_runtime_t *runtime = context;
    int fps = GetFPS();
    return fps > 0 ? (uint32_t)fps : runtime->app->target_fps;
}

raylib_lite_result_t raylib_lite_game_app_run(
    const raylib_lite_game_app_t *app)
{
    if (!app || !app->on_render || !app->target_fps || !app->logic_hz ||
            !app->platform.clock.monotonic_us ||
            !app->platform.clock.sleep_for_us)
        return RAYLIB_LITE_INVALID_ARGUMENT;

    mosaico_game_config_t game = MOSAICO_GAME_CONFIG_DEFAULT();
    game.target_fps = (int)app->target_fps;
    if (MosaicoGameInit(&game) != 0) return RAYLIB_LITE_PLATFORM_ERROR;
    mosaico_action_reset();
    bool started = false;
    raylib_lite_result_t result = mosaico_raylib_port_init_backend(
        &app->platform.video);
    if (result != RAYLIB_LITE_OK) goto shutdown_game;

    uint16_t width = 0, height = 0;
    mosaico_raylib_port_get_dimensions(&width, &height);
    if (!width || !height) {
        result = RAYLIB_LITE_INVALID_STATE;
        goto shutdown_port;
    }
    InitWindow((int)width, (int)height,
               app->window_title ? app->window_title : "Raylib Lite");
    SetTargetFPS((int)app->target_fps);
    if (app->on_start) {
        /* Once entered, on_start owns a matching on_stop even when startup
         * reports failure after partially acquiring application resources. */
        started = true;
        result = app->on_start(app->user);
        if (result != RAYLIB_LITE_OK) goto shutdown_port;
    }
    app->on_render(app->user);
    result = last_frame_result();
    if (frame_result_is_fatal(result)) goto shutdown_port;
    result = mosaico_raylib_port_flush(3000);
    if (result != RAYLIB_LITE_OK) goto shutdown_port;
    if (app->on_first_present) {
        result = app->on_first_present(app->user);
        if (result != RAYLIB_LITE_OK) goto shutdown_port;
    }

    app_runtime_t runtime = {
        .app = app,
        .terminal_result = RAYLIB_LITE_OK,
    };
    raylib_lite_runner_config_t runner = {
        .user = &runtime,
        .logic_hz = app->logic_hz,
        .target_fps = app->target_fps,
        .should_close = should_close,
        .idle = idle,
        .poll_input = poll_input,
        .update = update,
        .render = render,
        .render_fps = render_fps,
    };
    result = raylib_lite_runner_run(&runner, &app->platform.clock, NULL);
    if (result == RAYLIB_LITE_OK &&
            runtime.terminal_result != RAYLIB_LITE_OK)
        result = runtime.terminal_result;

shutdown_port:
    if (started && app->on_stop) app->on_stop(app->user);
    CloseWindow();
    mosaico_raylib_port_deinit();
shutdown_game:
    MosaicoGameShutdown();
    return result;
}
