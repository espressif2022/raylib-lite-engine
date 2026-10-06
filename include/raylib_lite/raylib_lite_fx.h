// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { RAYLIB_LITE_EASE_LINEAR,RAYLIB_LITE_EASE_IN,RAYLIB_LITE_EASE_OUT,RAYLIB_LITE_EASE_IN_OUT } raylib_lite_easing_t;
typedef struct { float from,to;uint32_t duration,elapsed;raylib_lite_easing_t easing;bool active; } raylib_lite_tween_t;
float raylib_lite_ease(raylib_lite_easing_t easing,float progress);
void raylib_lite_tween_start(raylib_lite_tween_t *t,float from,float to,uint32_t ticks,raylib_lite_easing_t easing);
float raylib_lite_tween_tick(raylib_lite_tween_t *t);
typedef struct { float x,y,vx,vy,gravity;uint32_t color;uint16_t life;bool active; } raylib_lite_particle_t;
typedef struct { raylib_lite_particle_t *items;size_t capacity,cursor; } raylib_lite_particle_pool_t;
void raylib_lite_particle_pool_init(raylib_lite_particle_pool_t *pool,raylib_lite_particle_t *items,size_t capacity);
raylib_lite_particle_t *raylib_lite_particle_spawn(raylib_lite_particle_pool_t *pool,raylib_lite_particle_t value);
void raylib_lite_particle_pool_update(raylib_lite_particle_pool_t *pool);
#ifdef __cplusplus
}
#endif
