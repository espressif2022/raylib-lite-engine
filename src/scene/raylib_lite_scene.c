// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_scene.h"
#include <string.h>

enum { TRANSITION_PUSH, TRANSITION_POP, TRANSITION_REPLACE };

void raylib_lite_scene_stack_init(raylib_lite_scene_stack_t *stack)
{
    if (stack) memset(stack, 0, sizeof(*stack));
}

static void execute_transition(raylib_lite_scene_stack_t *stack,
                               raylib_lite_scene_transition_t transition)
{
    raylib_lite_scene_entry_t old;
    switch (transition.operation) {
    case TRANSITION_PUSH:
        if (stack->count) {
            old = stack->entries[stack->count - 1];
            if (old.scene->pause) old.scene->pause(old.context);
        }
        stack->entries[stack->count++] = transition.entry;
        if (transition.entry.scene->enter)
            transition.entry.scene->enter(transition.entry.context);
        break;
    case TRANSITION_POP:
        old = stack->entries[--stack->count];
        stack->entries[stack->count] = (raylib_lite_scene_entry_t){0};
        if (old.scene->exit) old.scene->exit(old.context);
        if (stack->count) {
            old = stack->entries[stack->count - 1];
            if (old.scene->resume) old.scene->resume(old.context);
        }
        break;
    case TRANSITION_REPLACE:
        if (stack->count) {
            old = stack->entries[stack->count - 1];
            if (old.scene->exit) old.scene->exit(old.context);
            stack->entries[stack->count - 1] = transition.entry;
        } else {
            stack->entries[stack->count++] = transition.entry;
        }
        if (transition.entry.scene->enter)
            transition.entry.scene->enter(transition.entry.context);
        break;
    }
}

static void begin_dispatch(raylib_lite_scene_stack_t *stack)
{
    stack->dispatching = true;
    stack->projected_count = stack->count;
    stack->pending_count = 0;
}

static void finish_dispatch(raylib_lite_scene_stack_t *stack)
{
    /* pending_count is also this dispatch's budget: never recycle slots while
     * draining, otherwise an enter -> replace -> enter loop could run forever. */
    for (size_t i = 0; i < stack->pending_count; ++i)
        execute_transition(stack, stack->pending[i]);
    memset(stack->pending, 0, sizeof(stack->pending));
    stack->pending_count = 0;
    stack->projected_count = stack->count;
    stack->dispatching = false;
}

static bool request_transition(raylib_lite_scene_stack_t *stack,
                               unsigned operation,
                               const raylib_lite_scene_t *scene, void *context)
{
    if (!stack || (operation != TRANSITION_POP && !scene)) return false;
    size_t depth = stack->dispatching ? stack->projected_count : stack->count;
    if (operation == TRANSITION_PUSH) {
        if (depth >= RAYLIB_LITE_SCENE_STACK_CAPACITY) return false;
        ++depth;
    } else if (operation == TRANSITION_POP) {
        if (!depth) return false;
        --depth;
    } else if (!depth) {
        ++depth;
    }
    raylib_lite_scene_transition_t transition = {operation, {scene, context}};
    if (stack->dispatching) {
        if (stack->pending_count >= RAYLIB_LITE_SCENE_TRANSITION_CAPACITY) return false;
        stack->pending[stack->pending_count++] = transition;
        stack->projected_count = depth;
    } else {
        begin_dispatch(stack);
        stack->projected_count = depth;
        execute_transition(stack, transition);
        finish_dispatch(stack);
    }
    return true;
}

bool raylib_lite_scene_push(raylib_lite_scene_stack_t *stack,
                            const raylib_lite_scene_t *scene, void *context)
{
    return request_transition(stack, TRANSITION_PUSH, scene, context);
}

bool raylib_lite_scene_pop(raylib_lite_scene_stack_t *stack)
{
    return request_transition(stack, TRANSITION_POP, NULL, NULL);
}

bool raylib_lite_scene_replace(raylib_lite_scene_stack_t *stack,
                               const raylib_lite_scene_t *scene, void *context)
{
    return request_transition(stack, TRANSITION_REPLACE, scene, context);
}

bool raylib_lite_scene_event(raylib_lite_scene_stack_t *stack, const void *event)
{
    if (!stack || !stack->count || stack->dispatching) return false;
    begin_dispatch(stack);
    raylib_lite_scene_entry_t entry = stack->entries[stack->count - 1];
    bool consumed = entry.scene->event && entry.scene->event(entry.context, event);
    finish_dispatch(stack);
    return consumed;
}

void raylib_lite_scene_update(raylib_lite_scene_stack_t *stack)
{
    if (!stack || !stack->count || stack->dispatching) return;
    begin_dispatch(stack);
    raylib_lite_scene_entry_t entry = stack->entries[stack->count - 1];
    if (entry.scene->update) entry.scene->update(entry.context);
    finish_dispatch(stack);
}

void raylib_lite_scene_render(raylib_lite_scene_stack_t *stack)
{
    if (!stack || stack->dispatching) return;
    begin_dispatch(stack);
    for (size_t i = 0; i < stack->count; ++i) {
        raylib_lite_scene_entry_t entry = stack->entries[i];
        if (entry.scene->render) entry.scene->render(entry.context);
    }
    finish_dispatch(stack);
}
