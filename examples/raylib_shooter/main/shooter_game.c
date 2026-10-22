// SPDX-License-Identifier: Apache-2.0
#include "shooter_game.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static uint32_t next_random(shooter_game_t *game)
{
    uint32_t x = game->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return game->rng = x ? x : 0x6d2b79f5U;
}

static bool overlaps(const shooter_actor_t *a, float aw, float ah,
                     const shooter_actor_t *b, float bw, float bh)
{
    return a->x < b->x + bw && a->x + aw > b->x &&
           a->y < b->y + bh && a->y + ah > b->y;
}

static float arena_width(float width)
{
    return width >= 480.0f && width <= 2048.0f ? width : 480.0f;
}

/* Spawn columns are authored for the 480-wide arena and spread to the
 * current width; 480 maps 1:1. */
static float spread(const shooter_game_t *game, float x)
{
    return x * (game->width - 28.0f) / 452.0f;
}

void shooter_game_reset(shooter_game_t *game, uint32_t seed)
{
    const float width = arena_width(game->width);
    memset(game, 0, sizeof(*game));
    game->width = width;
    game->phase = SHOOTER_START;
    game->player = (shooter_actor_t){.x=width*.5f-18.0f, .y=420, .active=true};
    game->lives = 3;
    game->wave = 1;
    game->wave_banner = 60;
    game->rng = seed ? seed : 1;
}

void shooter_game_start(shooter_game_t *game)
{
    if (game->phase != SHOOTER_START && game->phase != SHOOTER_GAME_OVER && game->phase != SHOOTER_WON) return;
    bool down = game->pointer_down;
    shooter_game_reset(game, game->rng);
    game->phase = SHOOTER_PLAYING;
    game->pointer_down = down;
    game->pointer_consumed = down;
}

void shooter_game_set_pointer(shooter_game_t *game, float x, float y, bool pressed)
{
    bool fresh = pressed && !game->pointer_down;
    game->pointer_down = pressed;
    if (!pressed) { game->pointer_consumed = false; return; }
    if (fresh) {
        if (game->phase == SHOOTER_START ||
            ((game->phase == SHOOTER_GAME_OVER || game->phase == SHOOTER_WON) && x >= game->width*.5f-160 &&
             x < game->width*.5f+160 && y >= 296 && y < 342)) {
            shooter_game_start(game);
            game->pointer_consumed = true;
        } else if (game->phase == SHOOTER_PAUSED ||
                   (game->phase == SHOOTER_PLAYING && x >= game->width-68 &&
                    x < game->width-12 && y >= 72 && y < 128)) {
            shooter_game_toggle_pause(game);
            game->pointer_consumed = true;
        }
    }
    if (game->phase == SHOOTER_PLAYING && !game->pointer_consumed) {
        /* Relative drag anchors each fresh contact, keeping the ship visible
         * away from the finger and preventing lift/repress teleportation. */
        if (!fresh) shooter_game_move(game, x-game->pointer_x, y-game->pointer_y);
    }
    game->pointer_x=x; game->pointer_y=y;
}

void shooter_game_move(shooter_game_t *game, float dx, float dy)
{
    if (!game || game->phase != SHOOTER_PLAYING) return;
    game->player.x = fmaxf(0, fminf(game->width - 36, game->player.x + dx));
    game->player.y = fmaxf(280, fminf(438, game->player.y + dy));
}

void shooter_game_set_width(shooter_game_t *game, float width)
{
    if (!game) return;
    width = arena_width(width);
    if (width == game->width) return;
    game->width = width;
    if (game->phase == SHOOTER_START) game->player.x = width * .5f - 18.0f;
    else game->player.x = fmaxf(0, fminf(width - 36, game->player.x));
}

void shooter_game_toggle_pause(shooter_game_t *game)
{
    if (game->phase == SHOOTER_PLAYING) game->phase = SHOOTER_PAUSED;
    else if (game->phase == SHOOTER_PAUSED) game->phase = SHOOTER_PLAYING;
}

static void spawn_bullet(shooter_game_t *game)
{
    const bool twin=game->combo>=4;
    const unsigned count=twin?2U:1U;
    const float offsets[2]={9.0f,25.0f};
    for(unsigned shot=0;shot<count;++shot){
        for (size_t i=0; i<SHOOTER_MAX_BULLETS; ++i) if (!game->bullets[i].active) {
            game->bullets[i] = (shooter_actor_t){
                .x=game->player.x+(twin?offsets[shot]:17.0f),
                .y=game->player.y-13, .vy=-7.4f,
                .kind=(uint8_t)(game->combo>=8), .active=true};
            ++game->shots_fired;
            break;
        }
    }
}

static bool spawn_enemy_at(shooter_game_t *game,float x,uint8_t kind,bool elite)
{
    for (size_t i=0; i<SHOOTER_MAX_ENEMIES; ++i) if (!game->enemies[i].active) {
        uint32_t value=next_random(game);
        const float speed=1.05f+(float)game->wave*0.075f;
        uint8_t hp=(uint8_t)(elite?8U:kind+1U);
        game->enemies[i]=(shooter_actor_t){.x=x, .y=-36,
            .vx=kind==1?((value>>9)&1?1.15f:-1.15f):0,
            .vy=(speed+(kind==0?0.45f:(kind==2?-0.18f:0.12f)))*(elite?.72f:1.0f),
            .kind=kind, .hp=hp, .max_hp=hp, .attack_ticks=60, .elite=elite, .active=true};
        return true;
    }
    return false;
}

static void spawn_wave_enemy(shooter_game_t *game)
{
    uint32_t value=next_random(game);
    uint8_t kind=(uint8_t)((value>>5)%3U);
    static const float lanes[5]={42.0f,132.0f,222.0f,312.0f,402.0f};
    uint8_t step=game->formation_step++;
    float x=game->wave < 2 ? 14.0f+(float)(value%418U)
                           : lanes[(step+(game->wave&1U))%5U];
    bool elite=game->wave>=2 && step%10U==9U;
    if(elite){kind=2;x=222.0f;}
    (void)spawn_enemy_at(game,spread(game,x),kind,elite);
    if(game->wave>=3 && step%5U==4U)
        (void)spawn_enemy_at(game,spread(game,454.0f-x),0,false);
}

static void emit_particles(shooter_game_t *game,float x,float y,uint8_t color,unsigned count)
{
    for(unsigned made=0;made<count;++made){
        for(size_t i=0;i<SHOOTER_MAX_PARTICLES;++i)if(!game->particles[i].active){
            uint32_t value=next_random(game);
            float angle=(float)(value&255U)*0.024543693f;
            float speed=0.9f+(float)((value>>8)&15U)*0.14f;
            game->particles[i]=(shooter_particle_t){
                .x=x,.y=y,.vx=cosf(angle)*speed,.vy=sinf(angle)*speed,
                .life=(uint8_t)(14U+((value>>16)&15U)),.color=color,.active=true};
            break;
        }
    }
}

static void hurt_player(shooter_game_t *game)
{
    if (game->invulnerable || game->phase != SHOOTER_PLAYING) return;
    game->hit_flash=8; game->invulnerable=60;
    /* Keep baseline firepower after damage; combo remains a scoring reward. */
    game->combo=0; game->combo_timer=0;
    if (game->lives && --game->lives==0) game->phase=SHOOTER_GAME_OVER;
}

static void enemy_attack(shooter_game_t *game, const shooter_actor_t *enemy)
{
    unsigned count=enemy->boss?5U:enemy->kind==2?3U:1U;
    float center=enemy->boss?34.0f:14.0f;
    for(unsigned shot=0;shot<count;++shot) {
        for(unsigned i=0;i<SHOOTER_MAX_ENEMY_BULLETS;++i) {
            if(game->enemy_bullets[i].active) continue;
            float spread_x=(float)shot-(float)(count-1)*.5f;
            game->enemy_bullets[i]=(shooter_actor_t){
                .x=enemy->x+center-4, .y=enemy->y+center,
                .vx=spread_x*.85f, .vy=2.1f, .active=true};
            break;
        }
    }
}

static void begin_finale(shooter_game_t *game)
{
    if(game->finale_spawned) return;
    for(unsigned i=0;i<SHOOTER_MAX_ENEMIES;++i) if(!game->enemies[i].active) {
        game->enemies[i]=(shooter_actor_t){.x=game->width*.5f-34,.y=90,
            .vx=1.5f,.hp=48,.max_hp=48,.kind=2,.boss=true,.elite=true,
            .attack_ticks=90,.active=true};
        game->finale_spawned=true;
        game->wave_banner=72;
        return;
    }
}

void shooter_game_update(shooter_game_t *game)
{
    if (game->phase != SHOOTER_PLAYING) return;
    ++game->tick;
    uint8_t next_wave=(uint8_t)(1U+game->tick/900U);
    if(next_wave>3)next_wave=3;
    if(next_wave!=game->wave){game->wave=next_wave;game->wave_banner=72;}
    if(game->wave_banner)--game->wave_banner;
    if(game->hit_flash)--game->hit_flash;
    if(game->invulnerable)--game->invulnerable;
    if(game->combo_timer){
        if(!--game->combo_timer)game->combo=0;
    }
    if (!game->fire_cooldown) {
        spawn_bullet(game);
        game->fire_cooldown=(uint16_t)(game->combo>=8?6:8);
    }
    else --game->fire_cooldown;
    if (game->tick >= SHOOTER_FINALE_TICK) begin_finale(game);
    else if (!game->spawn_cooldown) {
        spawn_wave_enemy(game);
        unsigned pace=game->wave<7?(unsigned)(7-game->wave):0U;
        game->spawn_cooldown=(uint16_t)(10U+pace+next_random(game)%13U);
    }
    else if (game->spawn_cooldown) --game->spawn_cooldown;
    for (size_t i=0; i<SHOOTER_MAX_BULLETS; ++i) {
        shooter_actor_t *b=&game->bullets[i]; if (!b->active) continue;
        b->y+=b->vy; if (b->y < -12) b->active=false;
    }
    for(size_t i=0;i<SHOOTER_MAX_PARTICLES;++i){
        shooter_particle_t *p=&game->particles[i];if(!p->active)continue;
        p->x+=p->vx;p->y+=p->vy;p->vy+=0.025f;
        if(p->life)--p->life;else p->active=false;
    }
    for (size_t i=0; i<SHOOTER_MAX_ENEMIES; ++i) {
        shooter_actor_t *e=&game->enemies[i]; if (!e->active) continue;
        e->x+=e->vx; e->y+=e->vy;
        float size=e->boss?68.0f:28.0f;
        if (e->x<0 || e->x>game->width-size) e->vx=-e->vx;
        if (e->y>490) { e->active=false; continue; } /* Missed enemies cost points, not hull. */
        shooter_actor_t core={.x=game->player.x+14,.y=game->player.y+14};
        if (!game->invulnerable&&overlaps(e,size,size,&core,8,8)) {
            if(!e->boss) e->active=false;
            emit_particles(game,e->x+14,e->y+14,3,12);
            hurt_player(game);
            continue;
        }
        if(e->y>35 && e->y<250) {
            if(e->attack_ticks) --e->attack_ticks;
            else { enemy_attack(game,e); e->attack_ticks=e->boss?75:120; }
        }
        for (size_t j=0; j<SHOOTER_MAX_BULLETS; ++j) {
            shooter_actor_t *b=&game->bullets[j];
            if (b->active && overlaps(e,size,size,b,6,12)) {
                b->active=false;
                if(e->hp)--e->hp;
                emit_particles(game,b->x,e->y+18,e->kind,3);
                if(!e->hp){
                    e->active=false;
                    if(e->boss && game->phase==SHOOTER_PLAYING) game->phase=SHOOTER_WON;
                    if(game->combo<99)++game->combo;
                    if(game->combo>game->max_combo)game->max_combo=game->combo;
                    game->combo_timer=90;++game->kills;
                    uint32_t multiplier=1U+(uint32_t)game->combo/4U;
                    game->score+=(uint32_t)(15U+e->kind*20U+(e->elite?100U:0U))*multiplier;
                    emit_particles(game,e->x+14,e->y+14,e->kind,12U+e->kind*4U);
                }
                break;
            }
        }
    }
    for(unsigned i=0;i<SHOOTER_MAX_ENEMY_BULLETS;++i) {
        shooter_actor_t *b=&game->enemy_bullets[i];
        if(!b->active) continue;
        b->x+=b->vx; b->y+=b->vy;
        if(b->y>480 || b->x < -8 || b->x>game->width) { b->active=false; continue; }
        shooter_actor_t core={.x=game->player.x+14,.y=game->player.y+14};
        if(overlaps(b,8,8,&core,8,8)) { b->active=false; hurt_player(game); }
    }
}

uint32_t shooter_game_state_hash(const shooter_game_t *game)
{
    const uint8_t *bytes=(const uint8_t *)game; uint32_t hash=2166136261U;
    for (size_t i=0; i<sizeof(*game); ++i) hash=(hash^bytes[i])*16777619U;
    return hash;
}
