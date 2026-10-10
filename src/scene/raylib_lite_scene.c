// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_scene.h"
#include <string.h>
void raylib_lite_scene_stack_init(raylib_lite_scene_stack_t *s){if(s)memset(s,0,sizeof(*s));}
bool raylib_lite_scene_push(raylib_lite_scene_stack_t *s,const raylib_lite_scene_t *v,void *c){if(!s||!v||s->count==RAYLIB_LITE_SCENE_STACK_CAPACITY)return false;if(s->count&&s->entries[s->count-1].scene->pause)s->entries[s->count-1].scene->pause(s->entries[s->count-1].context);s->entries[s->count++]=(raylib_lite_scene_entry_t){v,c};if(v->enter)v->enter(c);return true;}
bool raylib_lite_scene_pop(raylib_lite_scene_stack_t *s){if(!s||!s->count)return false;raylib_lite_scene_entry_t e=s->entries[--s->count];if(e.scene->exit)e.scene->exit(e.context);if(s->count&&s->entries[s->count-1].scene->resume)s->entries[s->count-1].scene->resume(s->entries[s->count-1].context);return true;}
bool raylib_lite_scene_replace(raylib_lite_scene_stack_t *s,const raylib_lite_scene_t *v,void *c){if(!s||!v)return false;if(s->count){raylib_lite_scene_entry_t e=s->entries[s->count-1];if(e.scene->exit)e.scene->exit(e.context);s->entries[s->count-1]=(raylib_lite_scene_entry_t){v,c};}else{s->entries[s->count++]=(raylib_lite_scene_entry_t){v,c};}if(v->enter)v->enter(c);return true;}
bool raylib_lite_scene_event(raylib_lite_scene_stack_t *s,const void *e){if(!s||!s->count)return false;raylib_lite_scene_entry_t *v=&s->entries[s->count-1];return v->scene->event&&v->scene->event(v->context,e);}
void raylib_lite_scene_update(raylib_lite_scene_stack_t *s){if(s&&s->count){raylib_lite_scene_entry_t *v=&s->entries[s->count-1];if(v->scene->update)v->scene->update(v->context);}}
void raylib_lite_scene_render(raylib_lite_scene_stack_t *s){if(!s)return;for(size_t i=0;i<s->count;++i)if(s->entries[i].scene->render)s->entries[i].scene->render(s->entries[i].context);}
