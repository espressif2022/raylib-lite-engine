#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "tower_game.h"
#include "shooter_game.h"

static void test_tower_synergy(void)
{
    tower_game_t game;
    tower_game_reset(&game, 7);
    game.phase = TOWER_PLAYING;
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
    shooter_game_t game;
    shooter_game_reset(&game, 11);
    game.phase = SHOOTER_PLAYING;
    bool saw_elite = false;
    for (unsigned frame = 0; frame < 900 && !saw_elite; ++frame) {
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

int main(void)
{
    test_tower_synergy();
    test_shooter_elite_cycle();
    puts("gameplay improvements: ok");
    return 0;
}
