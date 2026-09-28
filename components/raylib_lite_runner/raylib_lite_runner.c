// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_runner.h"

#include <limits.h>
#include <string.h>

#define CREDIT_ONE 1000000ULL
#define MAX_LOGIC_UPDATES 3U
#define IDLE_SLEEP_US 1000ULL

static uint64_t credit_add(uint64_t credit, uint64_t elapsed, uint32_t rate)
{
    if (elapsed && rate > (UINT64_MAX - credit) / elapsed)
        return UINT64_MAX;
    return credit + elapsed * rate;
}

raylib_lite_result_t raylib_lite_runner_run(
    const raylib_lite_runner_config_t *config,
    const raylib_lite_clock_t *clock,
    raylib_lite_runner_stats_t *out_stats)
{
    if (!config || !clock || !clock->monotonic_us ||
            !clock->sleep_for_us || !config->should_close ||
            !config->update || !config->render || config->logic_hz == 0 ||
            config->target_fps == 0)
        return RAYLIB_LITE_INVALID_ARGUMENT;

    raylib_lite_runner_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    uint64_t previous_us = clock->monotonic_us(clock->context);
    uint64_t logic_credit = CREDIT_ONE;
    uint64_t render_credit = CREDIT_ONE;

    while (!config->should_close(config->user)) {
        ++stats.scheduler_passes;
        if (config->poll_input) config->poll_input(config->user);

        if (config->idle && config->idle(config->user)) {
            ++stats.idle_passes;
            logic_credit = CREDIT_ONE;
            render_credit = CREDIT_ONE;
            clock->sleep_for_us(clock->context, IDLE_SLEEP_US);
            previous_us = clock->monotonic_us(clock->context);
            continue;
        }

        uint64_t now_us = clock->monotonic_us(clock->context);
        uint64_t elapsed_us = now_us - previous_us;
        previous_us = now_us;
        uint32_t render_fps = config->render_fps
            ? config->render_fps(config->user) : 0;
        if (render_fps == 0) render_fps = config->target_fps;
        logic_credit = credit_add(logic_credit, elapsed_us, config->logic_hz);
        render_credit = credit_add(render_credit, elapsed_us, render_fps);

        uint64_t due_logic = logic_credit / CREDIT_ONE;
        if (due_logic > MAX_LOGIC_UPDATES) {
            stats.discarded_logic_updates += due_logic - MAX_LOGIC_UPDATES;
            logic_credit = MAX_LOGIC_UPDATES * CREDIT_ONE +
                           logic_credit % CREDIT_ONE;
        }
        uint64_t due_render = render_credit / CREDIT_ONE;
        if (due_render > 1) {
            stats.discarded_render_frames += due_render - 1;
            render_credit = CREDIT_ONE + render_credit % CREDIT_ONE;
        }

        while (logic_credit >= CREDIT_ONE) {
            config->update(config->user);
            ++stats.logic_updates;
            logic_credit -= CREDIT_ONE;
        }
        if (render_credit >= CREDIT_ONE) {
            config->render(config->user);
            ++stats.rendered_frames;
            render_credit -= CREDIT_ONE;
        }

        if (logic_credit < CREDIT_ONE && render_credit < CREDIT_ONE) {
            uint64_t logic_wait = (CREDIT_ONE - logic_credit +
                                   config->logic_hz - 1U) / config->logic_hz;
            uint64_t render_wait = (CREDIT_ONE - render_credit +
                                    render_fps - 1U) / render_fps;
            uint64_t wait_us = logic_wait < render_wait ? logic_wait : render_wait;
            clock->sleep_for_us(clock->context, wait_us);
        }
    }
    if (out_stats) *out_stats = stats;
    return RAYLIB_LITE_OK;
}
