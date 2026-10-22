// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform_game.h"

static void place(platform_game_t *game, float x, float y)
{
    game->player_x = x;
    game->player_y = y;
    game->velocity_x = game->velocity_y = 0;
}

static void test_movement_levels_and_coin_chain(void)
{
    platform_game_t game;
    platform_game_reset(&game);
    assert(game.phase == PLATFORM_START && game.lives == 3 && game.level == 0);
    assert(game.coin_count == 12 && game.enemy_count == 4);
    size_t block_count = 0;
    assert(platform_game_blocks(&game, &block_count) != NULL && block_count == 12);
    assert(strcmp(platform_game_level_name(&game), "SUNNY MEADOW") == 0);
    platform_game_set_pointer(&game, 220, 430, true, 1);
    assert(game.phase == PLATFORM_PLAYING && game.move_right && !game.facing_left);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_START);
    for (int i = 0; i < 20; ++i) platform_game_update(&game);
    assert(game.player_x > 42);
    platform_game_set_pointer(&game, 350, 430, true, 2);
    platform_game_update(&game);
    assert(game.velocity_y < 0 && !game.grounded && game.velocity_x > 0);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_JUMP);
    platform_game_set_pointer(&game, 40, 430, true, 1);
    assert(game.move_left && game.facing_left && game.jump_held);
    platform_game_set_pointer(&game, 350, 430, false, 2);
    platform_game_set_pointer(&game, 40, 430, false, 1);
    uint32_t landing = 0;
    for (int i = 0; i < 40 && !game.grounded; ++i) {
        platform_game_update(&game);
        landing |= platform_game_take_events(&game);
    }
    assert(game.grounded && (landing & PLATFORM_EVENT_LAND) && game.land_air_ticks >= 6);
    platform_game_set_action(&game, PLATFORM_ACTION_JUMP, true);
    platform_game_update(&game);
    float held_jump_velocity = game.velocity_y;
    platform_game_set_action(&game, PLATFORM_ACTION_JUMP, false);
    assert(game.velocity_y < 0 && game.velocity_y > held_jump_velocity);
    assert(!game.grounded);
    platform_game_set_action(&game, PLATFORM_ACTION_PAUSE, true);
    assert(game.phase == PLATFORM_PAUSED);
    uint32_t paused_tick = game.tick;
    platform_game_update(&game);
    assert(game.tick == paused_tick);
    platform_game_set_action(&game, PLATFORM_ACTION_PAUSE, true);
    assert(game.phase == PLATFORM_PLAYING);
    uint32_t pause_events = platform_game_take_events(&game);
    assert((pause_events & PLATFORM_EVENT_PAUSE) && (pause_events & PLATFORM_EVENT_RESUME));

    size_t spring_count = 0;
    const platform_spring_t *springs = platform_game_springs(&game, &spring_count);
    assert(springs != NULL && spring_count == 2);
    platform_game_set_action(&game, PLATFORM_ACTION_LEFT, false);
    platform_game_set_action(&game, PLATFORM_ACTION_RIGHT, false);
    place(&game, springs[0].x + 4, springs[0].y - 36);
    game.grounded = true;
    platform_game_update(&game);
    assert(game.velocity_y < -10 && !game.grounded);
    assert(game.spring_ticks[0] > 0);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_SPRING);

    float checkpoint = platform_game_checkpoint_x(&game);
    place(&game, checkpoint + 1, 354);
    platform_game_update(&game);
    assert(game.checkpoint_active && game.respawn_x == checkpoint);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_CHECKPOINT);
    uint8_t checkpoint_lives = game.lives;
    game.player_y = 501;
    platform_game_update(&game);
    assert(game.lives + 1 == checkpoint_lives && game.player_x == checkpoint);
    assert(game.invulnerable_ticks == PLATFORM_INVULNERABLE_TICKS);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_HURT);

    game.player_x = platform_game_finish_x(&game) + 1;
    platform_game_update(&game);
    assert(game.phase == PLATFORM_LEVEL_CLEAR && game.level == 0);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_LEVEL_CLEAR);
    const uint32_t score = game.score;
    const uint8_t lives = game.lives;
    platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
    assert(game.phase == PLATFORM_PLAYING && game.level == 1);
    assert(game.score == score && game.lives == lives);
    assert(game.coin_count == 12 && game.enemy_count == 5);
    assert(game.coins_collected == 0 && game.invulnerable_ticks == 0);
    assert(platform_game_world_width(&game) == 2400.0f);
    springs = platform_game_springs(&game, &spring_count);
    assert(springs != NULL && spring_count == 3);
    place(&game, 1092, 354);
    game.grounded = true;
    platform_game_update(&game);
    assert(game.velocity_y < -10 && !game.grounded);

    for (uint8_t level = 1; level < PLATFORM_LEVEL_COUNT; ++level) {
        game.player_x = platform_game_finish_x(&game) + 1;
        platform_game_update(&game);
        if (level + 1 == PLATFORM_LEVEL_COUNT) break;
        assert(game.phase == PLATFORM_LEVEL_CLEAR);
        platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
        assert(game.phase == PLATFORM_PLAYING && game.level == level + 1);
    }
    assert(game.phase == PLATFORM_WON && game.level == 3);
    assert(game.coin_count == 15 && game.enemy_count == 7);
    assert(platform_game_world_width(&game) == 2760.0f);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_WON);
    platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
    assert(game.phase == PLATFORM_PLAYING && game.level == 0 && game.lives == 3);

    uint32_t chain_score = game.score;
    place(&game, game.coins[0].x - 6, game.coins[0].y - 8);
    platform_game_update(&game);
    assert(game.coin_chain == 1 && game.score == chain_score + 100U);
    assert(game.coins_collected == 1 && game.last_gain == 100);
    place(&game, game.coins[1].x - 6, game.coins[1].y - 8);
    platform_game_update(&game);
    assert(game.coin_chain == 2 && game.score == chain_score + 300U);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_COIN);
    assert(platform_game_take_events(&game) == 0);
    assert(platform_game_state_hash(&game) != 0);
}

static void drop_on_enemy(platform_game_t *game, size_t index)
{
    platform_enemy_t *enemy = &game->enemies[index];
    game->player_x = enemy->x;
    game->player_y = enemy->y - 36 + 6;
    game->velocity_x = 0;
    game->velocity_y = 3.0f;
    game->grounded = false;
}

static void test_stomp_chain_and_invulnerability(void)
{
    platform_game_t game;
    platform_game_reset(&game);
    platform_game_set_action(&game, PLATFORM_ACTION_RESTART, true);
    (void)platform_game_take_events(&game);

    /* Consecutive stomps without landing double the reward. */
    drop_on_enemy(&game, 0);
    game.enemies[0].speed = 0;
    platform_game_update(&game);
    assert(!game.enemies[0].active && game.stomp_chain == 1 && game.last_gain == 250);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_STOMP);
    drop_on_enemy(&game, 1);
    game.enemies[1].speed = 0;
    platform_game_update(&game);
    assert(!game.enemies[1].active && game.stomp_chain == 2 && game.last_gain == 500);
    assert(game.score == 750 && game.stomps == 2);

    /* After a hit the player respawns and cannot be hit again immediately. */
    platform_enemy_t *enemy = &game.enemies[2];
    enemy->speed = 0;
    place(&game, enemy->x, enemy->y);
    game.grounded = true;
    uint8_t lives = game.lives;
    platform_game_update(&game);
    assert(game.lives + 1 == lives && game.invulnerable_ticks > 0);
    assert(platform_game_take_events(&game) & PLATFORM_EVENT_HURT);
    place(&game, enemy->x, enemy->y);
    platform_game_update(&game);
    assert(game.lives + 1 == lives);

    /* Running out of lives reports game over exactly once. */
    game.invulnerable_ticks = 0;
    game.lives = 1;
    place(&game, enemy->x, enemy->y);
    platform_game_update(&game);
    assert(game.phase == PLATFORM_GAME_OVER);
    uint32_t events = platform_game_take_events(&game);
    assert((events & PLATFORM_EVENT_GAME_OVER) && !(events & PLATFORM_EVENT_HURT));
    assert(platform_game_take_events(&game) == 0);
}

int main(void)
{
    test_movement_levels_and_coin_chain();
    test_stomp_chain_and_invulnerability();
    puts("platform game model: ok");
    return 0;
}
