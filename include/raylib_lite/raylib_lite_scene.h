// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RAYLIB_LITE_SCENE_STACK_CAPACITY 8
#define RAYLIB_LITE_SCENE_TRANSITION_CAPACITY 8

typedef struct raylib_lite_scene {
    void (*enter)(void *); void (*exit)(void *); void (*pause)(void *);
    void (*resume)(void *); bool (*event)(void *,const void *);
    void (*update)(void *); void (*render)(void *);
} raylib_lite_scene_t;
typedef struct { const raylib_lite_scene_t *scene; void *context; } raylib_lite_scene_entry_t;
typedef struct {
    unsigned operation;
    raylib_lite_scene_entry_t entry;
} raylib_lite_scene_transition_t;
typedef struct {
    raylib_lite_scene_entry_t entries[RAYLIB_LITE_SCENE_STACK_CAPACITY];
    size_t count;
    /* Internal state: do not change these fields, entries, or count directly. */
    raylib_lite_scene_transition_t pending[RAYLIB_LITE_SCENE_TRANSITION_CAPACITY];
    size_t pending_count;
    size_t projected_count;
    bool dispatching;
} raylib_lite_scene_stack_t;

/* Initialize only a fresh or completely unwound stack, never from a callback. */
void raylib_lite_scene_stack_init(raylib_lite_scene_stack_t *stack);
/* Outside callbacks, transitions execute immediately. Within any callback on
 * this stack, true accepts a FIFO transition after the outermost call's callback
 * traversal finishes. Validation uses the projected depth including requests
 * already accepted. At most TRANSITION_CAPACITY deferred requests are accepted
 * per outer call, including requests made while committing earlier transitions.
 * false has no effect. Borrowed scene/context must survive until their exit;
 * accepted requests must not be manually freed even before their enter runs.
 * A transition's callbacks may themselves change the eventual top scene. */
bool raylib_lite_scene_push(raylib_lite_scene_stack_t *stack,const raylib_lite_scene_t *scene,void *context);
bool raylib_lite_scene_pop(raylib_lite_scene_stack_t *stack);
bool raylib_lite_scene_replace(raylib_lite_scene_stack_t *stack,const raylib_lite_scene_t *scene,void *context);
/* Dispatch is synchronous; nested dispatch on the same stack is ignored (event
 * returns false). Render traverses the original stack before committing requests.
 * This API is single-threaded; initialization is not a transition or a reset. */
bool raylib_lite_scene_event(raylib_lite_scene_stack_t *stack,const void *event);
void raylib_lite_scene_update(raylib_lite_scene_stack_t *stack);
void raylib_lite_scene_render(raylib_lite_scene_stack_t *stack);
#ifdef __cplusplus
}
#endif
