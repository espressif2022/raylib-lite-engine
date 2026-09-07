// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MOSAICO_SCENE_STACK_CAPACITY 8
typedef struct mosaico_scene {
    void (*enter)(void *); void (*exit)(void *); void (*pause)(void *);
    void (*resume)(void *); bool (*event)(void *,const void *);
    void (*update)(void *); void (*render)(void *);
} mosaico_scene_t;
typedef struct { const mosaico_scene_t *scene; void *context; } mosaico_scene_entry_t;
typedef struct { mosaico_scene_entry_t entries[MOSAICO_SCENE_STACK_CAPACITY]; size_t count; } mosaico_scene_stack_t;
void mosaico_scene_stack_init(mosaico_scene_stack_t *stack);
bool mosaico_scene_push(mosaico_scene_stack_t *stack,const mosaico_scene_t *scene,void *context);
bool mosaico_scene_pop(mosaico_scene_stack_t *stack);
bool mosaico_scene_replace(mosaico_scene_stack_t *stack,const mosaico_scene_t *scene,void *context);
bool mosaico_scene_event(mosaico_scene_stack_t *stack,const void *event);
void mosaico_scene_update(mosaico_scene_stack_t *stack);
void mosaico_scene_render(mosaico_scene_stack_t *stack);
#ifdef __cplusplus
}
#endif
