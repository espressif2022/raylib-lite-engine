// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { MOSAICO_EASE_LINEAR,MOSAICO_EASE_IN,MOSAICO_EASE_OUT,MOSAICO_EASE_IN_OUT } mosaico_easing_t;
typedef struct { float from,to;uint32_t duration,elapsed;mosaico_easing_t easing;bool active; } mosaico_tween_t;
float mosaico_ease(mosaico_easing_t easing,float progress);
void mosaico_tween_start(mosaico_tween_t *t,float from,float to,uint32_t ticks,mosaico_easing_t easing);
float mosaico_tween_tick(mosaico_tween_t *t);
typedef struct { float x,y,vx,vy,gravity;uint32_t color;uint16_t life;bool active; } mosaico_particle_t;
typedef struct { mosaico_particle_t *items;size_t capacity,cursor; } mosaico_particle_pool_t;
void mosaico_particle_pool_init(mosaico_particle_pool_t *pool,mosaico_particle_t *items,size_t capacity);
mosaico_particle_t *mosaico_particle_spawn(mosaico_particle_pool_t *pool,mosaico_particle_t value);
void mosaico_particle_pool_update(mosaico_particle_pool_t *pool);
#ifdef __cplusplus
}
#endif
