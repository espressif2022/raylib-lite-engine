#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tower_game.h"
#include "shooter_game.h"

static void test_tower_synergy(void)
{
    tower_game_t game;
    tower_game_reset(&game, 7);
    game.phase = TOWER_PLAYING;
    game.awaiting_wave = false;
    game.intermission = 100;
    game.enemies[0] = (tower_enemy_t){
        .x = 100, .y = 100, .hp = 100, .max_hp = 100,
        .speed = 0, .waypoint = 1, .slow_ticks = 20, .active = true,
    };
    game.projectiles[0] = (tower_projectile_t){
        .x = 100, .y = 100, .target = 0, .damage = 10,
        .kind = TOWER_PULSE, .ttl = 2, .active = true,
    };
    tower_game_update(&game);
    assert(game.enemies[0].hp == 80);
    assert(game.synergy_hits == 1);
}

static void test_shooter_elite_cycle(void)
{
    shooter_game_t game = {0};
    shooter_game_reset(&game, 11);
    game.phase = SHOOTER_PLAYING;
    bool saw_elite = false;
    for (unsigned frame = 0; frame < 1800 && !saw_elite; ++frame) {
        game.lives = 255;
        shooter_game_update(&game);
        for (unsigned i = 0; i < SHOOTER_MAX_ENEMIES; ++i) {
            if (game.enemies[i].active && game.enemies[i].elite) {
                assert(game.enemies[i].kind == 2);
                assert(game.enemies[i].max_hp == 8);
                saw_elite = true;
            }
        }
    }
    assert(saw_elite);
}

static void tower_tap(tower_game_t *game, float x, float y)
{
    tower_game_set_pointer(game,x,y,true);
    tower_game_set_pointer(game,x,y,false);
}

static void test_tower_purchase_confirmation(void)
{
    tower_game_t game;
    tower_game_reset(&game,7);
    tower_tap(&game,240,220);
    tower_tap(&game,58,154);
    assert(game.selected_pad == 0 && game.credits == 190 && !game.towers[0].occupied);
    tower_tap(&game,58,154);
    assert(game.credits == 190); /* Inspection never spends, even when tapped twice. */
    tower_game_set_pointer(&game,200,90,true);
    assert(game.credits == 120 && game.towers[0].level == 1 && game.selected_pad == -1);
    tower_game_set_pointer(&game,200,90,true);
    assert(game.credits == 120); /* Held confirmation does not buy another level. */
    tower_game_set_pointer(&game,200,90,false);
    tower_tap(&game,58,154);
    assert(tower_game_purchase(&game).cost == 65);
    tower_tap(&game,400,90);
    assert(game.selected_pad == -1 && game.credits == 120);
    tower_tap(&game,58,154);
    tower_tap(&game,200,90);
    assert(game.towers[0].level == 2 && game.credits == 55);
    tower_tap(&game,58,154);
    tower_tap(&game,393,437);
    tower_purchase_t quote = tower_game_purchase(&game);
    assert(quote.action == TOWER_FEEDBACK_REPLACED && quote.refund == 67 && quote.cost == 53);
    assert(game.towers[0].type == TOWER_PULSE && game.credits == 55);
    tower_tap(&game,200,90);
    assert(game.towers[0].type == TOWER_FROST && game.towers[0].level == 1 && game.credits == 2);
    tower_tap(&game,195,130);
    tower_tap(&game,200,90);
    assert(!game.towers[1].occupied && game.credits == 2 && game.feedback == TOWER_FEEDBACK_NO_CREDITS);
    game.phase = TOWER_PAUSED;
    tower_tap(&game,200,90);
    assert(game.credits == 2);
}

static void test_shooter_touch_gestures(void)
{
    shooter_game_t game = {0};
    shooter_game_reset(&game,11);
    shooter_game_set_pointer(&game,100,100,true);
    assert(game.phase == SHOOTER_PLAYING && game.player.x == 222 && game.player.y == 420);
    shooter_game_set_pointer(&game,100,400,true);
    assert(game.player.x == 222); /* Deploy gesture is consumed until release. */
    shooter_game_set_pointer(&game,100,400,false);
    shooter_game_set_pointer(&game,100,400,true);
    assert(game.player.x == 222); /* A new anchor does not teleport the ship. */
    shooter_game_set_pointer(&game,80,390,true);
    assert(game.player.x == 202 && game.player.y == 410);
    game.phase = SHOOTER_GAME_OVER;
    shooter_game_set_pointer(&game,240,320,true);
    assert(game.phase == SHOOTER_GAME_OVER);
    shooter_game_set_pointer(&game,240,320,false);
    shooter_game_set_pointer(&game,10,400,true);
    assert(game.phase == SHOOTER_GAME_OVER); /* Retry requires the visible control. */
    shooter_game_set_pointer(&game,10,400,false);
    shooter_game_set_pointer(&game,240,320,true);
    assert(game.phase == SHOOTER_PLAYING && game.player.y == 420);
    shooter_game_set_pointer(&game,240,320,false);
    shooter_game_set_width(&game,640);
    shooter_game_set_pointer(&game,600,90,true);
    assert(game.phase == SHOOTER_PAUSED);
    shooter_game_set_pointer(&game,600,90,true);
    assert(game.phase == SHOOTER_PAUSED);
    shooter_game_set_pointer(&game,600,90,false);
    shooter_game_set_pointer(&game,240,320,true);
    assert(game.phase == SHOOTER_PLAYING && game.player.y == 420);
}

static unsigned enemy_shots(const shooter_game_t *game)
{
    unsigned count = 0;
    for (unsigned i = 0; i < SHOOTER_MAX_ENEMY_BULLETS; ++i)
        if (game->enemy_bullets[i].active) ++count;
    return count;
}

static void quiet_shooter(shooter_game_t *game)
{
    *game = (shooter_game_t){0};
    shooter_game_reset(game, 17);
    shooter_game_start(game);
    game->spawn_cooldown = 65535;
    game->fire_cooldown = 65535;
}

static void test_shooter_warning_and_core_collision(void)
{
    shooter_game_t game;
    quiet_shooter(&game);
    game.enemies[0] = (shooter_actor_t){.x=100, .y=80, .kind=2,
        .hp=3, .max_hp=3, .attack_ticks=20, .active=true};
    for (unsigned i = 0; i < 20; ++i) {
        shooter_game_update(&game);
        assert(enemy_shots(&game) == 0); /* Warning precedes the first salvo. */
    }
    shooter_game_toggle_pause(&game);
    unsigned tick = game.tick;
    for (unsigned i = 0; i < 60; ++i) shooter_game_update(&game);
    assert(game.tick == tick && enemy_shots(&game) == 0);
    shooter_game_toggle_pause(&game);
    shooter_game_update(&game);
    assert(enemy_shots(&game) == 3);
    assert(game.enemy_bullets[0].vx < 0 && game.enemy_bullets[1].vx == 0 &&
           game.enemy_bullets[2].vx > 0);

    quiet_shooter(&game);
    game.score = 100;
    game.enemies[0] = (shooter_actor_t){.x=10, .y=491, .hp=1, .active=true};
    shooter_game_update(&game);
    assert(game.lives == 3 && game.score == 100 && !game.enemies[0].active);
    /* Grazing the visible ship away from its small core is survivable. */
    game.enemy_bullets[0] = (shooter_actor_t){.x=game.player.x, .y=game.player.y, .active=true};
    shooter_game_update(&game);
    assert(game.lives == 3);
    game.enemy_bullets[0] = (shooter_actor_t){.x=game.player.x+14, .y=game.player.y+14, .active=true};
    shooter_game_update(&game);
    assert(game.lives == 2 && game.invulnerable > 0 && !game.enemy_bullets[0].active);
    game.enemy_bullets[0] = (shooter_actor_t){.x=game.player.x+14, .y=game.player.y+14, .active=true};
    shooter_game_update(&game);
    assert(game.lives == 2); /* A second overlapping bullet cannot drain another life. */
    game.invulnerable = 0;
    game.lives = 1;
    game.enemy_bullets[0] = (shooter_actor_t){.x=game.player.x+14, .y=game.player.y+14, .active=true};
    shooter_game_update(&game);
    assert(game.lives == 0 && game.phase == SHOOTER_GAME_OVER);
    uint32_t hash = shooter_game_state_hash(&game);
    for (unsigned i = 0; i < 30; ++i) shooter_game_update(&game);
    assert(shooter_game_state_hash(&game) == hash);
}

static void test_shooter_finite_boss_and_restart(void)
{
    shooter_game_t game;
    quiet_shooter(&game);
    game.tick = SHOOTER_FINALE_TICK - 2;
    shooter_game_update(&game);
    assert(!game.finale_spawned);
    shooter_game_update(&game);
    assert(game.tick == SHOOTER_FINALE_TICK && game.finale_spawned);
    unsigned bosses = 0, boss_index = 0;
    for (unsigned i = 0; i < SHOOTER_MAX_ENEMIES; ++i) {
        if (game.enemies[i].active && game.enemies[i].boss) { ++bosses; boss_index = i; }
    }
    assert(bosses == 1 && game.enemies[boss_index].hp > 1);
    /* A full normal-enemy pool delays rather than loses the finale request. */
    shooter_game_t crowded;
    quiet_shooter(&crowded);
    crowded.tick = SHOOTER_FINALE_TICK - 1;
    for (unsigned i = 0; i < SHOOTER_MAX_ENEMIES; ++i)
        crowded.enemies[i] = (shooter_actor_t){.x=20, .y=10, .hp=1, .active=true};
    shooter_game_update(&crowded);
    assert(!crowded.finale_spawned);
    crowded.enemies[5].active = false;
    shooter_game_update(&crowded);
    assert(crowded.finale_spawned && crowded.enemies[5].boss);

    shooter_actor_t *boss = &game.enemies[boss_index];
    boss->attack_ticks = 0;
    shooter_game_update(&game);
    assert(enemy_shots(&game) == 5);
    boss->hp = 1;
    boss->vx = 0;
    game.bullets[0] = (shooter_actor_t){.x=boss->x+20, .y=boss->y+20, .active=true};
    shooter_game_update(&game);
    assert(game.phase == SHOOTER_WON && game.kills == 1 && game.score > 0);
    uint32_t hash = shooter_game_state_hash(&game);
    for (unsigned i = 0; i < 90; ++i) shooter_game_update(&game);
    assert(shooter_game_state_hash(&game) == hash);
    shooter_game_set_pointer(&game, 240, 320, true);
    assert(game.phase == SHOOTER_PLAYING && game.tick == 0 && game.lives == 3);
    assert(!game.finale_spawned && game.score == 0 && enemy_shots(&game) == 0);
    shooter_game_set_pointer(&game, 0, 0, true);
    assert(game.player.x == 222 && game.player.y == 420); /* Retry contact is consumed. */
}

static void test_tower_all_pads_and_preparation(void)
{
    tower_game_t game;
    tower_game_reset(&game, 31);
    tower_tap(&game, 240, 220);
    for (unsigned i = 0; i < 300; ++i) tower_game_update(&game);
    assert(game.phase == TOWER_PLAYING && game.awaiting_wave && game.wave_spawned == 0);
    assert(game.credits == 190 && game.base_hp == 20 && game.intermission == 30);
    for (unsigned i = 0; i < TOWER_PAD_COUNT; ++i) {
        tower_tap(&game, game.towers[i].x, game.towers[i].y);
        assert(game.selected_pad == (int)i && game.awaiting_wave && game.credits == 190);
        tower_tap(&game, TOWER_CANCEL_X+10, TOWER_CONFIRM_Y+10);
        assert(game.selected_pad == -1);
    }
    tower_tap(&game, TOWER_WAVE_X+10, TOWER_WAVE_Y+10);
    assert(!game.awaiting_wave);
    for (unsigned i = 0; i < 31; ++i) tower_game_update(&game);
    assert(game.wave_spawned == 1);
}

static void test_tower_six_wave_campaign(void)
{
    tower_game_t game;
    tower_game_reset(&game, 43);
    tower_tap(&game, 240, 220);
    /* Give this lifecycle fixture a strong defense. Purchases, spawning,
     * targeting, damage, rewards and every wave transition still run normally. */
    game.credits = 10000;
    for (unsigned pad = 0; pad < TOWER_PAD_COUNT; ++pad) {
        for (unsigned level = 1; level <= 3; ++level) {
            tower_tap(&game, game.towers[pad].x, game.towers[pad].y);
            tower_tap(&game, TOWER_CONFIRM_X+10, TOWER_CONFIRM_Y+10);
            assert(game.towers[pad].occupied && game.towers[pad].level == level);
        }
    }
    for (unsigned wave = 1; wave <= 6; ++wave) {
        assert(game.phase == TOWER_PLAYING && game.wave == wave && game.awaiting_wave);
        uint16_t credits = game.credits;
        for (unsigned tick = 0; tick < 90; ++tick) tower_game_update(&game);
        assert(game.wave == wave && game.awaiting_wave && game.wave_spawned == 0 && game.credits == credits);
        tower_tap(&game, TOWER_WAVE_X+10, TOWER_WAVE_Y+10);
        for (unsigned tick = 0; tick < 10000 && game.phase == TOWER_PLAYING && !game.awaiting_wave; ++tick)
            tower_game_update(&game);
        assert(game.base_hp > 0 && game.kills > 0 && game.score > 0);
        if (wave < 6) assert(game.phase == TOWER_PLAYING && game.wave == wave+1 && game.awaiting_wave);
    }
    assert(game.phase == TOWER_WON && game.wave == 6);
    uint32_t hash = tower_game_state_hash(&game);
    for (unsigned tick = 0; tick < 90; ++tick) tower_game_update(&game);
    assert(tower_game_state_hash(&game) == hash);
    tower_tap(&game, 240, 320);
    assert(game.phase == TOWER_PLAYING && game.wave == 1 && game.awaiting_wave && game.credits == 190);
    for (unsigned i = 0; i < TOWER_PAD_COUNT; ++i) assert(!game.towers[i].occupied);
}

static void test_tower_final_leak_is_a_loss(void)
{
    tower_game_t game;
    tower_game_reset(&game, 7);
    game.phase = TOWER_PLAYING;
    game.awaiting_wave = false;
    game.wave = 6;
    game.wave_spawned = game.wave_total = 1;
    game.base_hp = 1;
    game.enemies[0] = (tower_enemy_t){.x=498, .y=360, .waypoint=7, .active=true};
    tower_game_update(&game);
    assert(game.base_hp == 0 && game.phase == TOWER_GAME_OVER);
}

int main(void)
{
    test_tower_all_pads_and_preparation();
    test_tower_six_wave_campaign();
    test_tower_final_leak_is_a_loss();
    test_shooter_warning_and_core_collision();
    test_shooter_finite_boss_and_restart();
    test_tower_purchase_confirmation();
    test_shooter_touch_gestures();
    test_tower_synergy();
    test_shooter_elite_cycle();
    puts("gameplay improvements: ok");
    return 0;
}
