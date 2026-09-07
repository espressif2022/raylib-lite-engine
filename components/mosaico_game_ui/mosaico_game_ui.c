// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_ui.h"
#include "mosaico_raylib_fast.h"
#include <string.h>
void mosaico_ui_init(mosaico_ui_t *u){if(u){memset(u,0,sizeof(*u));u->focused=-1;}}
bool mosaico_ui_add(mosaico_ui_t *u,mosaico_ui_node_t n){if(!u||u->count==MOSAICO_UI_CAPACITY)return false;n.visible=true;u->nodes[u->count++]=n;return true;}
static bool hit(Rectangle r,float x,float y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
bool mosaico_ui_pointer(mosaico_ui_t *u,int32_t track,float x,float y,bool down){
    if(!u)return false;
    mosaico_ui_pointer_t *pointer=NULL;
    for(size_t i=0;i<MOSAICO_UI_POINTER_CAPACITY;++i)
        if(u->pointers[i].active&&u->pointers[i].track_id==track)pointer=&u->pointers[i];
    if(!pointer&&down)for(size_t i=0;i<MOSAICO_UI_POINTER_CAPACITY;++i)
        if(!u->pointers[i].active){pointer=&u->pointers[i];break;}
    bool used=false;
    if(pointer){
        for(size_t i=0;i<u->count;++i){
            mosaico_ui_node_t *n=&u->nodes[i];
            if(n->visible&&n->type==MOSAICO_UI_BUTTON&&hit(n->bounds,x,y)){
                used=true;
                if(!down&&hit(n->bounds,pointer->x,pointer->y)&&n->click)n->click(n->id,n->context);
            }
        }
        *pointer=(mosaico_ui_pointer_t){.track_id=track,.x=x,.y=y,.active=down};
    }
    for(size_t i=0;i<u->count;++i){
        mosaico_ui_node_t *n=&u->nodes[i];
        if(n->type!=MOSAICO_UI_BUTTON||!n->visible)continue;
        n->pressed=false;
        for(size_t p=0;p<MOSAICO_UI_POINTER_CAPACITY;++p)
            if(u->pointers[p].active&&hit(n->bounds,u->pointers[p].x,u->pointers[p].y))n->pressed=true;
    }
    return used;
}
bool mosaico_ui_action(mosaico_ui_t *u,int d,bool confirm){if(!u)return false;if(d){int start=u->focused;for(size_t n=0;n<u->count;++n){start=(start+d+(int)u->count)%(int)u->count;if(u->nodes[start].visible&&u->nodes[start].type==MOSAICO_UI_BUTTON){u->focused=start;break;}}}if(confirm&&u->focused>=0){mosaico_ui_node_t *n=&u->nodes[u->focused];if(n->click)n->click(n->id,n->context);return true;}return u->focused>=0;}
void mosaico_ui_draw(mosaico_ui_t *u,int fs){if(!u)return;for(size_t i=0;i<u->count;++i){mosaico_ui_node_t *n=&u->nodes[i];if(!n->visible)continue;if(n->type!=MOSAICO_UI_LABEL)DrawRectangle((int)n->bounds.x,(int)n->bounds.y,(int)n->bounds.width,(int)n->bounds.height,n->pressed?(Color){n->color.r/2,n->color.g/2,n->color.b/2,n->color.a}:n->color);if(n->text){int w=MeasureText(n->text,fs);DrawText(n->text,(int)(n->bounds.x+(n->bounds.width-w)/2),(int)(n->bounds.y+(n->bounds.height-fs)/2),fs,n->text_color);}}}
