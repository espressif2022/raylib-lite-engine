// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_fx.h"
#include <string.h>
float raylib_lite_ease(raylib_lite_easing_t e,float p){if(p<0)p=0;if(p>1)p=1;if(e==RAYLIB_LITE_EASE_IN)return p*p;if(e==RAYLIB_LITE_EASE_OUT)return 1-(1-p)*(1-p);if(e==RAYLIB_LITE_EASE_IN_OUT)return p<.5f?2*p*p:1-2*(1-p)*(1-p);return p;}
void raylib_lite_tween_start(raylib_lite_tween_t *t,float a,float b,uint32_t d,raylib_lite_easing_t e){if(t)*t=(raylib_lite_tween_t){a,b,d?d:1,0,e,true};}
float raylib_lite_tween_tick(raylib_lite_tween_t *t){if(!t)return 0;if(t->elapsed<t->duration)++t->elapsed;float p=raylib_lite_ease(t->easing,(float)t->elapsed/t->duration);if(t->elapsed>=t->duration)t->active=false;return t->from+(t->to-t->from)*p;}
void raylib_lite_particle_pool_init(raylib_lite_particle_pool_t *p,raylib_lite_particle_t *i,size_t n){if(p){*p=(raylib_lite_particle_pool_t){i,n,0};if(i)memset(i,0,n*sizeof(*i));}}
raylib_lite_particle_t *raylib_lite_particle_spawn(raylib_lite_particle_pool_t *p,raylib_lite_particle_t v){if(!p||!p->items||!p->capacity)return NULL;raylib_lite_particle_t *x=&p->items[p->cursor++%p->capacity];*x=v;x->active=true;return x;}
void raylib_lite_particle_pool_update(raylib_lite_particle_pool_t *p){if(!p)return;for(size_t i=0;i<p->capacity;++i){raylib_lite_particle_t *x=&p->items[i];if(!x->active)continue;x->x+=x->vx;x->y+=x->vy;x->vy+=x->gravity;if(x->life)--x->life;if(!x->life)x->active=false;}}
