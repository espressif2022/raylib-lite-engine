// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PIRATE_MAX_ACTORS 8
#define PIRATE_MAX_SPOTS  9
#define PIRATE_GRID_SIZE  7

typedef enum {
    PIRATE_STAGE_TITLE       = 0,
    PIRATE_STAGE_SWIPE_BLOCK = 1, /* Act 1: 划动木闸堵海盗 + 桥梁变道开关 */
    PIRATE_STAGE_FIND_HIDDEN = 2, /* Act 2: 双重线索推理找海盗 */
    PIRATE_STAGE_TRAP_GRID   = 3, /* Act 3: 7x7 圆石建房围困 + 双步冲刺蓄力解谜 */
    PIRATE_STAGE_CLEAR       = 4, /* 单幕过关横幅 */
    PIRATE_STAGE_UPGRADE     = 5, /* 海岛工坊：三选一科技升级！ */
    PIRATE_STAGE_GAME_OVER   = 6,
} pirate_stage_t;

typedef enum {
    PIRATE_MOD_CALM_BREEZE = 0, /* 晴朗微风 */
    PIRATE_MOD_TAILWIND    = 1, /* 顺风加速 */
    PIRATE_MOD_JUMPER_RAID = 2, /* 跳跳精锐 */
    PIRATE_MOD_MISTY_TIDE  = 3, /* 浓雾涨潮 */
} pirate_modifier_t;

/* Act 2 Logic Deduction Clue Rules: players must match BOTH hat & prop! */
typedef enum {
    PIRATE_CLUE_BLACK_SWORD = 0, /* 线索:黑帽 + 持弯刀 */
    PIRATE_CLUE_PURPLE_BOMB = 1, /* 线索:紫色头巾 + 抱炸药桶 */
    PIRATE_CLUE_STRAW_COIN  = 2, /* 线索:偷戴草帽 + 拿金币 */
} pirate_clue_t;

/* Roguelite Island Upgrades between rounds */
typedef enum {
    PIRATE_UPG_WIDE_GATE   = 0, /* 加宽木闸：碰撞范围与弹簧击退增强 */
    PIRATE_UPG_SEARCHLIGHT = 1, /* 鹰眼探照灯：开局自动揭露 1 名真凶并+4秒 */
    PIRATE_UPG_EXTRA_WALLS = 2, /* 预筑路障：第三幕开局额外多放 2 座小屋 */
    PIRATE_UPG_REPAIR_HULL = 3, /* 加固宝库：立刻恢复 1 点生命并 +250 分 */
} pirate_upgrade_id_t;

typedef struct {
    bool active;
    uint8_t kind;     /* 0: black-hat scout, 1: purple jumper, 2: friendly straw-hat villager */
    uint8_t lane;     /* 0..2 horizontal plank walkway */
    float x, y;
    float vx, vy;
    bool jumped_col0;
    bool switched_bridge;
    uint8_t dunk_timer;
} pirate_runner_t;

typedef struct {
    float x, y;
    uint8_t cover_kind;   /* 0: cottage, 1: diamond tree, 2: berry bush, 3: lighthouse */
    uint8_t suspect_type; /* 0: black_sword, 1: purple_bomb, 2: straw_coin, 3: villager_letter, 4: villager_blue */
    bool is_target;       /* true ONLY if suspect_type matches current clue_rule! */
    bool caught;
    bool auto_revealed;   /* Highlighted by Searchlight upgrade */
    uint8_t peek_timer;
    uint8_t cooldown;
    uint8_t shake_ticks;
} pirate_spot_t;

typedef struct {
    pirate_stage_t stage;
    pirate_stage_t next_stage;
    pirate_modifier_t modifier;
    pirate_clue_t clue_rule;
    uint32_t rng;
    uint32_t tick;
    uint32_t score;
    uint32_t high_score;
    uint16_t round;
    uint8_t hp;
    uint8_t combo;
    uint16_t stage_timer;
    float wind_x;
    uint8_t toast_kind;
    uint8_t toast_ticks;
    float toast_x, toast_y;

    /* Player Upgrade Levels (persist across rounds in a run!) */
    uint8_t upg_wide_gate;
    uint8_t upg_searchlight;
    uint8_t upg_extra_walls;
    uint8_t upgrade_choices[3];

    /* Act 1: Swipe-to-Block Gates + Puzzle Bridge Switch */
    float gate_y[2];
    float gate_target_y[2];
    int8_t drag_gate;
    uint8_t bridge_dir;    /* 0: straight, 1: diverts lane 0 <-> lane 1, 2: diverts lane 1 <-> lane 2 */
    uint8_t quota_left;
    uint8_t spawn_cooldown;
    pirate_runner_t runners[PIRATE_MAX_ACTORS];

    /* Act 2: Find Disguised Pirates by Clue Deduction */
    uint8_t targets_total;
    uint8_t targets_found;
    pirate_spot_t spots[PIRATE_MAX_SPOTS];

    /* Act 3: 7x7 Hex-Grid Trap the Pirate + Dash Charge & Beacon */
    uint8_t grid[PIRATE_GRID_SIZE][PIRATE_GRID_SIZE]; /* 0: empty, 1: cottage, 2: tree, 3: barrel, 4: golden beacon */
    int8_t pirate_r, pirate_c;
    int8_t escape_next_r, escape_next_c; /* Preview of pirate's intended next step */
    float pirate_anim_x, pirate_anim_y;
    uint8_t moves_used;
    uint8_t dash_charge; /* Every 3rd turn, pirate charges a 2-step dash unless blocked! */
} pirate_game_t;

void pirate_game_reset(pirate_game_t *g, uint32_t seed);
void pirate_game_start_act(pirate_game_t *g, pirate_stage_t act);
void pirate_game_apply_upgrade(pirate_game_t *g, unsigned slot);
void pirate_game_update(pirate_game_t *g);
void pirate_game_pointer(pirate_game_t *g, float x, float y, bool pressed, bool fresh);
void pirate_game_action(pirate_game_t *g, int code, bool pressed);
void pirate_game_imu(pirate_game_t *g, float gx, float gy);
uint32_t pirate_game_state_hash(const pirate_game_t *g);
