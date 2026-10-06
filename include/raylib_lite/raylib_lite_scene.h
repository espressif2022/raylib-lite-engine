// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RAYLIB_LITE_SCENE_STACK_CAPACITY 8
typedef struct raylib_lite_scene {
    void (*enter)(void *); void (*exit)(void *); void (*pause)(void *);
    void (*resume)(void *); bool (*event)(void *,const void *);
    void (*update)(void *); void (*render)(void *);
} raylib_lite_scene_t;
typedef struct { const raylib_lite_scene_t *scene; void *context; } raylib_lite_scene_entry_t;
typedef struct { raylib_lite_scene_entry_t entries[RAYLIB_LITE_SCENE_STACK_CAPACITY]; size_t count; } raylib_lite_scene_stack_t;
void raylib_lite_scene_stack_init(raylib_lite_scene_stack_t *stack);
bool raylib_lite_scene_push(raylib_lite_scene_stack_t *stack,const raylib_lite_scene_t *scene,void *context);
bool raylib_lite_scene_pop(raylib_lite_scene_stack_t *stack);
bool raylib_lite_scene_replace(raylib_lite_scene_stack_t *stack,const raylib_lite_scene_t *scene,void *context);
bool raylib_lite_scene_event(raylib_lite_scene_stack_t *stack,const void *event);
void raylib_lite_scene_update(raylib_lite_scene_stack_t *stack);
void raylib_lite_scene_render(raylib_lite_scene_stack_t *stack);
#ifdef __cplusplus
}
#endif
