// SPDX-License-Identifier: Apache-2.0
#include <stddef.h>
#include "raylib_lite_design_view.h"
#include "raylib_lite_raylib.h"
#include "shooter_view.h"
#include "assets_ids.h"

static const Color C_CYAN={48,226,255,255};
static const Color C_PINK={255,60,166,255};
static const Color C_GOLD={255,190,62,255};
static const Color C_MINT={70,242,174,255};

/* The arena is 480 units tall and as wide as the panel; 480x480 is 1:1. */
#define FIELD 480.0f
static raylib_lite_design_view_t s_view={1.0f,480.0f,480.0f};
static int s_w=480,s_cx;
static Font s_zh={0};

static int font(int size){return raylib_lite_design_font(s_view,size);}
static void draw_text(const char *text,int x,int y,int size,Color color)
{
    DrawText(text,x,y,font(size),color);
}
static int measure_text(const char *text,int size){return MeasureText(text,font(size));}

static void draw_centered(const char *text,int y,int size,Color color)
{
    draw_text(text,(s_w-measure_text(text,size))/2,y,size,color);
}

static float zh_size(void)
{
    int base=s_zh.baseSize>0?s_zh.baseSize:16;
    if(raylib_lite_design_view_native(s_view)||s_view.scale<=0.0f)return (float)base;
    return (float)base/s_view.scale;
}
static float zh_spacing(void)
{
    if(raylib_lite_design_view_native(s_view)||s_view.scale<=0.0f)return 1.0f;
    return 1.0f/s_view.scale;
}
static void draw_zh(const char *text,int x,int y,Color color)
{
    if(s_zh.texture.id)
        DrawTextEx(s_zh,text,(Vector2){(float)x,(float)y},zh_size(),zh_spacing(),color);
}
static void draw_zh_centered(const char *text,int y,Color color)
{
    if(!s_zh.texture.id)return;
    float sz_px=zh_size(),sp=zh_spacing();
    Vector2 sz=MeasureTextEx(s_zh,text,sz_px,sp);
    DrawTextEx(s_zh,text,(Vector2){(float)s_w*.5f-sz.x*.5f,(float)y},sz_px,sp,color);
}

static void draw_background(const shooter_game_t *game,raylib_lite_atlas_t backdrop)
{
    const raylib_lite_sprite_frame_t *field=
        raylib_lite_atlas_get_frame(backdrop,RAYLIB_LITE_ASSET_ID_NEBULA_FIELD);
    /* Opaque 240x195 vista covering the play field above the ground strip:
     * exactly 2x at 480; wider arenas crop rows instead of stretching. */
    const float src_h=195.0f*480.0f/(float)s_w;
    if(field)DrawTexturePro(backdrop.texture,
                            (Rectangle){field->source.x,field->source.y+(195.0f-src_h)*.5f,240,src_h},
                            (Rectangle){0,0,(float)s_w,390},(Vector2){0,0},0,WHITE);
    else DrawRectangle(0,0,s_w,390,(Color){4,7,30,255});
    DrawRectangle(0,390,s_w,90,(Color){4,8,25,255});
    const int stars=68*s_w/480;
    for(int i=0;i<stars;++i){
        int depth=i%3+1;
        int x=(i*101+(i%7)*23)%s_w;
        int y=(i*53+(int)(game->tick*(unsigned)depth))%430;
        Color color=depth==3?(Color){196,240,255,255}:
                    (depth==2?(Color){93,150,210,255}:(Color){46,76,133,255});
        if(depth==3&&i%4==0)DrawRectangle(x,y,2,3,color);
        else DrawPixel(x,y,color);
    }
    for(int i=0;i*72<s_w+72;++i){
        int x=i*72-(int)(game->tick%72U);
        DrawRectangle(x,450,30,2,(Color){25,54,91,255});
        DrawRectangle(x+12,462,48,1,(Color){16,34,67,255});
    }
}

static void draw_sprite(raylib_lite_atlas_t atlas,raylib_lite_asset_id_t id,
                        float x,float y,float width,float height)
{
    const raylib_lite_sprite_frame_t *frame=raylib_lite_atlas_get_frame(atlas,id);
    if(!frame)return;
    DrawTexturePro(atlas.texture,frame->source,(Rectangle){x,y,width,height},
                   (Vector2){0,0},0,WHITE);
}

static void draw_ship(const shooter_game_t *game,raylib_lite_atlas_t atlas,float x,float y)
{
    if(game->invulnerable&&(game->tick&2U))return;
    DrawEllipse((int)x+18,(int)y+35,25,12,(Color){7,43,76,255});
    draw_sprite(atlas,RAYLIB_LITE_ASSET_ID_PLAYER_SHIP,x-9,y-8,54,58);
}

static void draw_enemy(raylib_lite_atlas_t atlas,const shooter_actor_t *enemy)
{
    static const raylib_lite_asset_id_t ids[]={
        RAYLIB_LITE_ASSET_ID_ENEMY_SCOUT,
        RAYLIB_LITE_ASSET_ID_ENEMY_ASSAULT,
        RAYLIB_LITE_ASSET_ID_ENEMY_TANK
    };
    static const Color colors[]={
        {255,105,62,255},{255,54,173,255},{72,238,170,255}
    };
    float size=enemy->boss?96.0f:enemy->elite?72.0f:(enemy->kind==0?43.0f:(enemy->kind==1?49.0f:55.0f));
    float hit_size=enemy->boss?68.0f:28.0f;
    DrawEllipse((int)(enemy->x+hit_size*.5f),(int)(enemy->y+hit_size*.5f+3),size*.43f,size*.31f,
                (Color){12,10,36,255});
    draw_sprite(atlas,ids[enemy->kind],enemy->x+(hit_size-size)*.5f,
                enemy->y+(hit_size-size)*.5f,size,size);
    if(enemy->elite)
        draw_text("ACE",(int)enemy->x+3,(int)enemy->y-22,10,C_GOLD);
    if(enemy->kind>0&&enemy->hp){
        DrawRectangle((int)enemy->x+3,(int)enemy->y-8,24,2,(Color){32,26,55,255});
        int hpw=enemy->max_hp?24*(int)enemy->hp/(int)enemy->max_hp:0;
        DrawRectangle((int)enemy->x+3,(int)enemy->y-8,hpw,2,
                      colors[enemy->kind]);
    }
}

static void draw_danger_cues(const shooter_game_t *game)
{
    for(size_t i=0;i<SHOOTER_MAX_ENEMIES;++i){
        const shooter_actor_t *enemy=&game->enemies[i];
        if(!enemy->active||enemy->y<330.0f)continue;
        Color color=enemy->y>400.0f?C_PINK:C_GOLD;
        int x=(int)enemy->x+14;
        DrawTriangle((Vector2){x,452},(Vector2){x-7,464},(Vector2){x+7,464},color);
    }
}

static void draw_projectiles(const shooter_game_t *game)
{
    for(unsigned i=0;i<SHOOTER_MAX_ENEMY_BULLETS;++i) if(game->enemy_bullets[i].active) {
        const shooter_actor_t *b=&game->enemy_bullets[i];
        DrawCircle((int)b->x+4,(int)b->y+4,6,C_GOLD);
        DrawCircle((int)b->x+4,(int)b->y+4,3,(Color){80,25,10,255});
    }
    for(unsigned i=0;i<SHOOTER_MAX_ENEMIES;++i) {
        const shooter_actor_t *e=&game->enemies[i];
        if(e->active && e->y>35 && e->y<250 && e->attack_ticks<=20)
            DrawCircleLines((int)e->x+(e->boss?34:14),(int)e->y+(e->boss?34:14),
                            14+(20-e->attack_ticks)*.5f,C_GOLD);
    }
    if(game->phase==SHOOTER_PLAYING)
        DrawCircleLines((int)game->player.x+18,(int)game->player.y+18,6,RAYWHITE);
    for(size_t i=0;i<SHOOTER_MAX_BULLETS;++i)if(game->bullets[i].active){
        const shooter_actor_t *b=&game->bullets[i];
        int x=(int)b->x,y=(int)b->y;
        Color core=b->kind?C_GOLD:(Color){202,252,255,255};
        DrawRectangle(x-2,y+7,10,12,(Color){9,55,88,255});
        DrawRectangle(x,y,6,16,C_CYAN);
        DrawRectangle(x+2,y-3,2,18,core);
    }
}

static void draw_particles(const shooter_game_t *game)
{
    static const Color colors[]={
        {255,114,50,255},{255,62,174,255},{70,242,174,255},{255,194,62,255}
    };
    for(size_t i=0;i<SHOOTER_MAX_PARTICLES;++i)if(game->particles[i].active){
        const shooter_particle_t *p=&game->particles[i];
        int size=p->life>19?3:(p->life>9?2:1);
        DrawRectangle((int)p->x,(int)p->y,size,size,colors[p->color&3U]);
    }
}

static void draw_hud(const shooter_game_t *game)
{
    const bool compact=!raylib_lite_design_view_native(s_view);
    DrawRectangle(s_w-68,72,56,56,(Color){9,15,43,255});
    DrawRectangleLines(s_w-68,72,56,56,C_CYAN);
    DrawRectangle(s_w-52,86,8,28,RAYWHITE);
    DrawRectangle(s_w-36,86,8,28,RAYWHITE);
    const int right=s_w-480;
    DrawRectangle(12,12,s_w-24,52,(Color){9,15,43,255});
    DrawRectangle(12,12,92,3,C_CYAN);
    DrawRectangle(376+right,61,92,3,C_PINK);
    if(compact&&s_zh.texture.id){
        /* At 320x240 (s_w=640, zoom=0.5), 32 design units = 16px on screen.
         * Place label and value side-by-side inside the 52-unit HUD bar. */
        draw_zh("得分",20,24,(Color){94,151,193,255});
        draw_text(TextFormat("%07lu",(unsigned long)game->score),84,30,19,RAYWHITE);
        draw_zh(TextFormat("战区%02u",game->wave),224,24,(Color){105,169,206,255});
        if(game->combo){
            Color color=game->combo>=8?C_GOLD:C_PINK;
            draw_text(TextFormat("x%02u",game->combo),354,30,19,color);
        }else{
            draw_zh("连击--",354,24,(Color){48,70,103,255});
        }
        draw_zh("护甲",492,24,(Color){117,150,184,255});
        for(unsigned i=0;i<3;++i){
            Color color=i<game->lives?C_MINT:(Color){40,35,67,255};
            int x=558+(int)i*20;
            DrawTriangle((Vector2){x+7,30},(Vector2){x,45},(Vector2){x+14,45},color);
        }
        return;
    }
    if(s_zh.texture.id)draw_zh("得分",25,18,(Color){94,151,193,255});
    else draw_text("SCORE",25,20,12,(Color){94,151,193,255});
    draw_text(TextFormat("%07lu",(unsigned long)game->score),25,36,19,RAYWHITE);
    if(s_zh.texture.id)draw_zh(TextFormat("战区 %02u",game->wave),200+s_cx,18,(Color){105,169,206,255});
    else draw_text(TextFormat("SECTOR %02u",game->wave),200+s_cx,20,13,(Color){105,169,206,255});
    if(game->combo){
        Color color=game->combo>=8?C_GOLD:C_PINK;
        draw_text(TextFormat("x%02u",game->combo),213+s_cx,39,19,color);
    }else if(s_zh.texture.id)draw_zh("连击 --",200+s_cx,38,(Color){48,70,103,255});
    else draw_text("LINK --",204+s_cx,40,12,(Color){48,70,103,255});
    if(s_zh.texture.id)draw_zh("护甲",390+right,18,(Color){117,150,184,255});
    else draw_text("HULL",390+right,20,12,(Color){117,150,184,255});
    for(unsigned i=0;i<3;++i){
        Color color=i<game->lives?C_MINT:(Color){40,35,67,255};
        int x=389+right+(int)i*20;
        DrawTriangle((Vector2){x+7,38},(Vector2){x,51},(Vector2){x+14,51},color);
    }
}

static void draw_corner_frame(Color accent)
{
    const int x=s_cx;
    DrawRectangle(28+x,139,424,210,(Color){5,10,31,255});
    DrawRectangle(28+x,139,424,2,(Color){22,73,105,255});
    DrawRectangle(28+x,347,424,2,(Color){55,22,78,255});
    DrawRectangle(28+x,139,22,4,accent);DrawRectangle(28+x,139,4,22,accent);
    DrawRectangle(430+x,345,22,4,accent);DrawRectangle(448+x,327,4,22,accent);
}

static void draw_start_card(void)
{
    const bool compact=!raylib_lite_design_view_native(s_view);
    draw_corner_frame(C_CYAN);
    draw_centered("MOSAICO",compact?146:154,18,(Color){97,160,202,255});
    draw_centered("STRIKE",compact?166:174,48,RAYWHITE);
    if(s_zh.texture.id)draw_zh_centered("星空突击",compact?210:224,C_GOLD);
    DrawRectangle(107+s_cx,246,266,2,(Color){38,86,121,255});
    if(s_zh.texture.id)draw_zh_centered("闪避弹幕 / 迎战王牌",compact?254:260,C_CYAN);
    else draw_centered("DODGE SHOTS / DEFEAT THE ACE",266,15,C_CYAN);
    DrawRectangleRounded((Rectangle){126.0f+s_cx,298,228,42},0.18f,4,C_CYAN);
    if(s_zh.texture.id)draw_zh_centered("点击出击",compact?303:311,(Color){3,22,35,255});
    else draw_centered("TOUCH TO DEPLOY",314,17,(Color){3,22,35,255});
    if(s_zh.texture.id)draw_zh_centered("拖动战机 / 自动开火",compact?364:375,(Color){91,139,180,255});
    else draw_centered("DRAG TO STEER  /  AUTO FIRE",379,15,(Color){91,139,180,255});
}

static void draw_status_card(const shooter_game_t *game,const char *title,const char *zh_title,Color accent)
{
    const bool compact=!raylib_lite_design_view_native(s_view);
    DrawRectangle(0,0,s_w,480,(Color){3,6,22,255});
    draw_corner_frame(accent);
    draw_centered(title,compact?156:168,36,RAYWHITE);
    if(s_zh.texture.id&&zh_title)draw_zh_centered(zh_title,compact?198:210,accent);
    if(game->phase==SHOOTER_GAME_OVER || game->phase==SHOOTER_WON){
        draw_centered(TextFormat("SCORE %07lu",(unsigned long)game->score),242,19,accent);
        draw_centered(TextFormat("%u KILLS  /  MAX LINK x%u",game->kills,game->max_combo),274,14,
                      (Color){126,165,197,255});
        DrawRectangleRounded((Rectangle){126.0f+s_cx,302,228,42},0.18f,4,accent);
        if(s_zh.texture.id)draw_zh_centered("点击重试",compact?307:315,RAYWHITE);
        else draw_centered("TOUCH TO RETRY",314,17,RAYWHITE);
    }else{
        draw_centered("COMBAT SYSTEMS STANDBY",246,15,(Color){132,162,194,255});
        DrawRectangleRounded((Rectangle){126.0f+s_cx,302,228,42},0.18f,4,accent);
        if(s_zh.texture.id)draw_zh_centered("点击继续",compact?307:315,(Color){28,18,5,255});
        else draw_centered("TOUCH TO RESUME",314,17,(Color){28,18,5,255});
    }
}

void shooter_view_render(const shooter_game_t *game,raylib_lite_atlas_t atlas,
                         raylib_lite_atlas_t backdrop,Font zh_font)
{
    if(!game)return;
    s_zh=zh_font;
    BeginDrawing();
    s_view=raylib_lite_design_view(FIELD);
    s_w=(int)(s_view.width+.5f);
    s_cx=(s_w-480)/2;
    const float field_w=(float)s_w;
    if(s_view.height>FIELD+1.0f){
        raylib_lite_design_begin(s_view);
        DrawRectangle(0,0,s_w,(int)s_view.height,(Color){3,6,22,255});
        raylib_lite_design_end(s_view);
    }
    raylib_lite_design_field_begin(s_view,field_w,FIELD);
    /* Status cards repaint the whole screen opaquely; skip the hidden scene. */
    if(game->phase!=SHOOTER_PAUSED&&game->phase!=SHOOTER_GAME_OVER&&game->phase!=SHOOTER_WON){
        draw_background(game,backdrop);
        if(game->phase!=SHOOTER_START)draw_ship(game,atlas,game->player.x,game->player.y);
        draw_projectiles(game);
        for(size_t i=0;i<SHOOTER_MAX_ENEMIES;++i)if(game->enemies[i].active)
            draw_enemy(atlas,&game->enemies[i]);
        draw_danger_cues(game);
        draw_particles(game);
        draw_hud(game);
    }
    if(game->phase==SHOOTER_START)draw_start_card();
    else if(game->phase==SHOOTER_PAUSED)draw_status_card(game,"PAUSED","暂停",C_GOLD);
    else if(game->phase==SHOOTER_GAME_OVER)draw_status_card(game,"MISSION LOST","任务失败",C_PINK);
    else if(game->phase==SHOOTER_WON)draw_status_card(game,"MISSION CLEAR","任务完成",C_MINT);
    else if(game->wave_banner){
        const bool compact=!raylib_lite_design_view_native(s_view);
        int width=game->wave_banner>54?250:(int)game->wave_banner*4;
        int bh=compact?36:28;
        DrawRectangle((s_w-width)/2,82,width,bh,(Color){8,27,52,255});
        if(s_zh.texture.id)
            draw_zh_centered((game->finale_spawned?"迎战王牌":"战区出击"),compact?84:88,C_CYAN);
        else
            draw_centered((game->finale_spawned?"FINAL ACE // DODGE AND FIRE":TextFormat("SECTOR %02u // ENGAGED",game->wave)),89,14,C_CYAN);
    }
    if(game->hit_flash){
        Color color=game->hit_flash&1U?C_PINK:C_GOLD;
        DrawRectangle(0,0,s_w,5,color);DrawRectangle(0,475,s_w,5,color);
        DrawRectangle(0,0,5,480,color);DrawRectangle(s_w-5,0,5,480,color);
    }
    raylib_lite_design_field_end(s_view,field_w,FIELD);
    EndDrawing();
}
