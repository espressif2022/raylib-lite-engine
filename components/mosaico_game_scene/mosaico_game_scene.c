// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_scene.h"
#include <string.h>
void mosaico_scene_stack_init(mosaico_scene_stack_t *s){if(s)memset(s,0,sizeof(*s));}
bool mosaico_scene_push(mosaico_scene_stack_t *s,const mosaico_scene_t *v,void *c){if(!s||!v||s->count==MOSAICO_SCENE_STACK_CAPACITY)return false;if(s->count&&s->entries[s->count-1].scene->pause)s->entries[s->count-1].scene->pause(s->entries[s->count-1].context);s->entries[s->count++]=(mosaico_scene_entry_t){v,c};if(v->enter)v->enter(c);return true;}
bool mosaico_scene_pop(mosaico_scene_stack_t *s){if(!s||!s->count)return false;mosaico_scene_entry_t e=s->entries[--s->count];if(e.scene->exit)e.scene->exit(e.context);if(s->count&&s->entries[s->count-1].scene->resume)s->entries[s->count-1].scene->resume(s->entries[s->count-1].context);return true;}
bool mosaico_scene_replace(mosaico_scene_stack_t *s,const mosaico_scene_t *v,void *c){if(!s||!v)return false;if(s->count){mosaico_scene_entry_t e=s->entries[s->count-1];if(e.scene->exit)e.scene->exit(e.context);s->entries[s->count-1]=(mosaico_scene_entry_t){v,c};}else{s->entries[s->count++]=(mosaico_scene_entry_t){v,c};}if(v->enter)v->enter(c);return true;}
bool mosaico_scene_event(mosaico_scene_stack_t *s,const void *e){if(!s||!s->count)return false;mosaico_scene_entry_t *v=&s->entries[s->count-1];return v->scene->event&&v->scene->event(v->context,e);}
void mosaico_scene_update(mosaico_scene_stack_t *s){if(s&&s->count){mosaico_scene_entry_t *v=&s->entries[s->count-1];if(v->scene->update)v->scene->update(v->context);}}
void mosaico_scene_render(mosaico_scene_stack_t *s){if(!s)return;for(size_t i=0;i<s->count;++i)if(s->entries[i].scene->render)s->entries[i].scene->render(s->entries[i].context);}
