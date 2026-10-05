// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_app.h"

#include "raylib_lite_runtime_stats.h"
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

static void poll_input(void *context)
{
    app_runtime_t *runtime = context;
    raylib_lite_input_event_t input;
    while (runtime->app->input &&
            raylib_lite_input_poll(runtime->app->input, &input)) {
        if (input.type == RAYLIB_LITE_INPUT_POINTER)
            MosaicoFastInjectPointer(0, input.x, input.y, input.pressed);
        else if (input.type == RAYLIB_LITE_INPUT_TOUCH)
            MosaicoFastInjectPointer(input.value, input.x, input.y,
                                     input.pressed);
        else if (input.type == RAYLIB_LITE_INPUT_IMU)
            MosaicoFastInjectImu(input.x / 1000.0f, input.y / 1000.0f,
                                 input.value / 1000.0f);
        mosaico_action_apply_event(&input);
        if (runtime->app->on_event)
            runtime->app->on_event(runtime->app->user, &input);
    }
    if (runtime->app->input)
        raylib_lite_runtime_stats_set_queue_overflows(
            raylib_lite_input_dropped(runtime->app->input));
}

static bool should_close(void *context)
{
    app_runtime_t *runtime = context;
    return runtime->terminal_result != RAYLIB_LITE_OK || MosaicoFastWindowShouldClose();
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
    uint64_t finished = runtime->app->platform.clock.monotonic_us(
        runtime->app->platform.clock.context);
    uint32_t elapsed = (uint32_t)(finished - started);
    runtime->update_us += elapsed;
    raylib_lite_runtime_stats_record_logic(finished, 0, elapsed);
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
    uint64_t finished = runtime->app->platform.clock.monotonic_us(
        runtime->app->platform.clock.context);
    uint32_t elapsed = (uint32_t)(finished - started);
    raylib_lite_runtime_stats_record_frame(
        finished, runtime->update_us, elapsed, 0,
        frame_result != RAYLIB_LITE_OK);
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
    int fps = MosaicoFastGetFPS();
    return fps > 0 ? (uint32_t)fps : runtime->app->target_fps;
}

raylib_lite_result_t raylib_lite_game_app_run(
    const raylib_lite_game_app_t *app)
{
    if (!app || !app->on_render || !app->target_fps || !app->logic_hz ||
            !app->platform.clock.monotonic_us ||
            !app->platform.clock.sleep_for_us)
        return RAYLIB_LITE_INVALID_ARGUMENT;

    raylib_lite_runtime_stats_reset();

    mosaico_action_reset();
    bool started = false;
    raylib_lite_result_t result = mosaico_raylib_port_init_backend(
        &app->platform.video);
    if (result != RAYLIB_LITE_OK) return result;

    uint16_t width = 0, height = 0;
    mosaico_raylib_port_get_dimensions(&width, &height);
    if (!width || !height) {
        result = RAYLIB_LITE_INVALID_STATE;
        goto shutdown_port;
    }
    MosaicoFastInitWindow((int)width, (int)height,
               app->window_title ? app->window_title : "Raylib Lite");
    MosaicoFastSetTargetFPS((int)app->target_fps);
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
        /* Over-budget games still yield periodically without paying a tick
         * on every frame. Tomb native: 30.3 -> 31.1 displayed FPS. */
        .overbudget_yield_every = 8,
    };
    result = raylib_lite_runner_run(&runner, &app->platform.clock, NULL);
    if (result == RAYLIB_LITE_OK &&
            runtime.terminal_result != RAYLIB_LITE_OK)
        result = runtime.terminal_result;

shutdown_port:
    if (started && app->on_stop) app->on_stop(app->user);
    MosaicoFastCloseWindow();
    mosaico_raylib_port_deinit();
    return result;
}
