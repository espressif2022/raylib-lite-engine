// SPDX-License-Identifier: Apache-2.0
#include "pirate_game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const float k_lane_y[3] = {152.0f, 256.0f, 360.0f};
static const float k_gate_x[2] = {168.0f, 296.0f};

static uint32_t next_rng(pirate_game_t *g)
{
    uint32_t x = g->rng ? g->rng : 0x50495241U;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g->rng = x ? x : 0x6d2b79f5U;
}

static void add_score(pirate_game_t *g, uint32_t pts)
{
    g->score += pts;
    if (g->score > g->high_score)
        g->high_score = g->score;
}

static void show_toast(pirate_game_t *g, uint8_t kind, float x, float y)
{
    g->toast_kind = kind;
    g->toast_ticks = 26U;
    g->toast_x = x;
    g->toast_y = y;
}

static void grid_screen_pos(int r, int c, float *out_x, float *out_y)
{
    *out_x = 84.0f + (float)c * 50.0f + ((r & 1) ? 24.0f : 0.0f);
    *out_y = 114.0f + (float)r * 46.0f;
}

static void get_hex_neighbors(int r, int c, int nr[6], int nc[6])
{
    const int even_dc[6] = {-1, 0, -1, 1, -1, 0};
    const int odd_dc[6]  = {0, 1, -1, 1, 0, 1};
    const int dr[6]      = {-1, -1, 0, 0, 1, 1};
    const int *dc = (r & 1) ? odd_dc : even_dc;
    for (int i = 0; i < 6; ++i) {
        nr[i] = r + dr[i];
        nc[i] = c + dc[i];
    }
}

static bool is_cell_blocked(uint8_t cell)
{
    return cell == 1 || cell == 2 || cell == 3;
}

/* BFS shortest-path solver for Act 3: returns true if the pirate has any valid
 * adjacent step, and writes the best escape step into (*step_r, *step_c). */
static bool choose_pirate_escape_step(pirate_game_t *g, int from_r, int from_c,
                                      int *step_r, int *step_c)
{
    int dist[PIRATE_GRID_SIZE][PIRATE_GRID_SIZE];
    int qr[49], qc[49];
    int head = 0, tail = 0;
    for (int r = 0; r < PIRATE_GRID_SIZE; ++r) {
        for (int c = 0; c < PIRATE_GRID_SIZE; ++c) {
            if (!is_cell_blocked(g->grid[r][c]) &&
                (r == 0 || r == PIRATE_GRID_SIZE - 1 || c == 0 || c == PIRATE_GRID_SIZE - 1)) {
                dist[r][c] = 0;
                qr[tail] = r;
                qc[tail++] = c;
            } else {
                dist[r][c] = 999;
            }
        }
    }
    while (head < tail) {
        int r = qr[head], c = qc[head++];
        int nr[6], nc[6];
        get_hex_neighbors(r, c, nr, nc);
        for (int i = 0; i < 6; ++i) {
            int rr = nr[i], cc = nc[i];
            if (rr < 0 || rr >= PIRATE_GRID_SIZE || cc < 0 || cc >= PIRATE_GRID_SIZE)
                continue;
            if (is_cell_blocked(g->grid[rr][cc]))
                continue;
            if (dist[rr][cc] > dist[r][c] + 1) {
                dist[rr][cc] = dist[r][c] + 1;
                qr[tail] = rr;
                qc[tail++] = cc;
            }
        }
    }

    int nr[6], nc[6];
    get_hex_neighbors(from_r, from_c, nr, nc);
    int best_d = 999;
    int candidates[6], count = 0;
    for (int i = 0; i < 6; ++i) {
        int rr = nr[i], cc = nc[i];
        if (rr < 0 || rr >= PIRATE_GRID_SIZE || cc < 0 || cc >= PIRATE_GRID_SIZE)
            continue;
        if (is_cell_blocked(g->grid[rr][cc]))
            continue;
        if (dist[rr][cc] < best_d) {
            best_d = dist[rr][cc];
            count = 0;
            candidates[count++] = i;
        } else if (dist[rr][cc] == best_d) {
            candidates[count++] = i;
        }
    }
    if (count == 0)
        return false;
    int pick = candidates[next_rng(g) % (uint32_t)count];
    *step_r = nr[pick];
    *step_c = nc[pick];
    return true;
}

static void update_escape_preview(pirate_game_t *g)
{
    int nr = g->pirate_r, nc = g->pirate_c;
    if (choose_pirate_escape_step(g, g->pirate_r, g->pirate_c, &nr, &nc)) {
        g->escape_next_r = (int8_t)nr;
        g->escape_next_c = (int8_t)nc;
    } else {
        g->escape_next_r = g->pirate_r;
        g->escape_next_c = g->pirate_c;
    }
}

static void roll_upgrade_choices(pirate_game_t *g)
{
    uint8_t pool[4] = {0, 1, 2, 3};
    for (unsigned i = 0; i < 3U; ++i) {
        unsigned j = i + (next_rng(g) % (4U - i));
        uint8_t tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
        g->upgrade_choices[i] = pool[i];
    }
}

void pirate_game_apply_upgrade(pirate_game_t *g, unsigned slot)
{
    if (!g || slot >= 3U) return;
    uint8_t upg = g->upgrade_choices[slot];
    if (upg == PIRATE_UPG_WIDE_GATE) {
        if (g->upg_wide_gate < 3U) ++g->upg_wide_gate;
    } else if (upg == PIRATE_UPG_SEARCHLIGHT) {
        if (g->upg_searchlight < 3U) ++g->upg_searchlight;
    } else if (upg == PIRATE_UPG_EXTRA_WALLS) {
        if (g->upg_extra_walls < 3U) ++g->upg_extra_walls;
    } else {
        if (g->hp < 3U) ++g->hp;
        add_score(g, 250U);
    }
    ++g->round;
    pirate_game_start_act(g, PIRATE_STAGE_SWIPE_BLOCK);
}

void pirate_game_start_act(pirate_game_t *g, pirate_stage_t act)
{
    g->stage = act;
    g->modifier = (pirate_modifier_t)(next_rng(g) % 4U);
    g->toast_kind = 0;
    g->toast_ticks = 0;

    if (act == PIRATE_STAGE_SWIPE_BLOCK) {
        memset(g->runners, 0, sizeof(g->runners));
        g->gate_y[0] = g->gate_target_y[0] = k_lane_y[0];
        g->gate_y[1] = g->gate_target_y[1] = k_lane_y[1];
        g->drag_gate = -1;
        g->bridge_dir = (uint8_t)(next_rng(g) % 3U);
        g->quota_left = (uint8_t)(6U + (g->round < 4U ? g->round * 2U : 8U));
        g->spawn_cooldown = 14U;
    } else if (act == PIRATE_STAGE_FIND_HIDDEN) {
        memset(g->spots, 0, sizeof(g->spots));
        g->clue_rule = (pirate_clue_t)(next_rng(g) % 3U);
        static const float base_xy[PIRATE_MAX_SPOTS][2] = {
            {78.0f, 268.0f},  {154.0f, 282.0f}, {238.0f, 234.0f},
            {322.0f, 276.0f}, {404.0f, 214.0f}, {114.0f, 358.0f},
            {214.0f, 368.0f}, {318.0f, 358.0f}, {410.0f, 352.0f},
        };
        static const uint8_t base_cover[PIRATE_MAX_SPOTS] = {
            0, 0, 1, 1, 3, 2, 0, 2, 1
        };
        for (int i = 0; i < PIRATE_MAX_SPOTS; ++i) {
            float jx = (float)((int)(next_rng(g) % 15U) - 7);
            float jy = (float)((int)(next_rng(g) % 9U) - 4);
            g->spots[i].x = base_xy[i][0] + jx;
            g->spots[i].y = base_xy[i][1] + jy;
            g->spots[i].cover_kind = base_cover[i];
            g->spots[i].cooldown = (uint8_t)(8U + (next_rng(g) % 30U));
            /* Non-target spots get either a friendly villager (3/4) or a
             * wrong-clue pirate (lookalike distractor!) for real deduction! */
            uint8_t distractor = (uint8_t)((g->clue_rule + 1U + (next_rng(g) & 1U)) % 3U);
            g->spots[i].suspect_type = (next_rng(g) & 1U) ? distractor : (uint8_t)(3U + (i & 1));
            g->spots[i].is_target = false;
        }
        g->targets_total = (uint8_t)(3U + (g->round % 3U));
        g->targets_found = 0;
        uint8_t indices[PIRATE_MAX_SPOTS];
        for (uint8_t i = 0; i < PIRATE_MAX_SPOTS; ++i) indices[i] = i;
        for (uint8_t i = 0; i < g->targets_total; ++i) {
            uint8_t swap_idx = (uint8_t)(i + (next_rng(g) % (PIRATE_MAX_SPOTS - i)));
            uint8_t tmp = indices[i];
            indices[i] = indices[swap_idx];
            indices[swap_idx] = tmp;
            uint8_t idx = indices[i];
            g->spots[idx].suspect_type = (uint8_t)g->clue_rule;
            g->spots[idx].is_target = true;
            g->spots[idx].peek_timer = (uint8_t)(48U + (next_rng(g) % 30U));
            if (i < g->upg_searchlight)
                g->spots[idx].auto_revealed = true;
        }
        g->stage_timer = (uint16_t)((18U + g->upg_searchlight * 4U) * 30U);
    } else if (act == PIRATE_STAGE_TRAP_GRID) {
        memset(g->grid, 0, sizeof(g->grid));
        g->pirate_r = 3;
        g->pirate_c = 3;
        grid_screen_pos(3, 3, &g->pirate_anim_x, &g->pirate_anim_y);
        g->moves_used = 0;
        g->dash_charge = 0;
        unsigned pre_blocks = 8U + (next_rng(g) % 3U) + g->upg_extra_walls * 2U;
        if (pre_blocks > 16U) pre_blocks = 16U;
        unsigned placed = 0;
        while (placed < pre_blocks) {
            int r = (int)(next_rng(g) % PIRATE_GRID_SIZE);
            int c = (int)(next_rng(g) % PIRATE_GRID_SIZE);
            if ((abs(r - 3) <= 1 && abs(c - 3) <= 1) || g->grid[r][c] != 0)
                continue;
            g->grid[r][c] = (uint8_t)(1U + (next_rng(g) % 3U));
            ++placed;
        }
        /* Place 2 Golden Beacons (value 4): building on a beacon spawns a
         * bonus adjacent tree automatically (2-for-1 puzzle combo!). */
        unsigned beacons = 0;
        for (int tries = 0; tries < 32 && beacons < 2U; ++tries) {
            int r = 1 + (int)(next_rng(g) % (PIRATE_GRID_SIZE - 2));
            int c = 1 + (int)(next_rng(g) % (PIRATE_GRID_SIZE - 2));
            if ((r == 3 && c == 3) || g->grid[r][c] != 0)
                continue;
            g->grid[r][c] = 4U;
            ++beacons;
        }
        update_escape_preview(g);
    } else if (act == PIRATE_STAGE_UPGRADE) {
        roll_upgrade_choices(g);
    }
}

void pirate_game_reset(pirate_game_t *g, uint32_t seed)
{
    uint32_t best = g ? g->high_score : 0;
    memset(g, 0, sizeof(*g));
    g->high_score = best;
    g->rng = seed ? seed : 0x50495241U;
    g->hp = 3U;
    g->round = 1U;
    pirate_game_start_act(g, PIRATE_STAGE_SWIPE_BLOCK);
    g->stage = PIRATE_STAGE_TITLE;
}

static void spawn_act1_runner(pirate_game_t *g)
{
    for (int i = 0; i < PIRATE_MAX_ACTORS; ++i) {
        if (g->runners[i].active) continue;
        uint32_t r = next_rng(g);
        uint8_t lane = (uint8_t)(r % 3U);
        uint8_t kind = 0;
        if ((r % 8U) == 0U) kind = 2; /* Friendly straw-hat villager */
        else if (g->modifier == PIRATE_MOD_JUMPER_RAID ? ((r % 3U) == 0U) : ((r % 4U) == 0U))
            kind = 1;
        float speed = 1.65f + (float)(r % 7U) * 0.14f + (float)g->round * 0.11f;
        if (g->modifier == PIRATE_MOD_TAILWIND) speed *= 1.20f;
        if (kind == 2) speed *= 0.90f;
        g->runners[i] = (pirate_runner_t){
            .active = true,
            .kind = kind,
            .lane = lane,
            .x = 42.0f,
            .y = k_lane_y[lane],
            .vx = speed,
            .vy = 0.0f,
        };
        break;
    }
}

void pirate_game_update(pirate_game_t *g)
{
    if (!g) return;
    ++g->tick;
    if (g->toast_ticks) --g->toast_ticks;

    float breeze = sinf((float)g->tick * 0.04f) * (g->modifier == PIRATE_MOD_TAILWIND ? 0.65f : 0.25f);
    g->wind_x = g->wind_x * 0.92f + breeze * 0.08f;

    if (g->stage == PIRATE_STAGE_TITLE || g->stage == PIRATE_STAGE_GAME_OVER ||
        g->stage == PIRATE_STAGE_UPGRADE)
        return;

    if (g->stage == PIRATE_STAGE_CLEAR) {
        if (g->stage_timer) {
            --g->stage_timer;
            if (g->stage_timer == 0U)
                pirate_game_start_act(g, g->next_stage);
        }
        return;
    }

    if (g->stage == PIRATE_STAGE_SWIPE_BLOCK) {
        for (int c = 0; c < 2; ++c) {
            g->gate_y[c] += (g->gate_target_y[c] - g->gate_y[c]) * 0.36f;
        }
        if (g->spawn_cooldown) {
            --g->spawn_cooldown;
        } else {
            spawn_act1_runner(g);
            g->spawn_cooldown = (uint8_t)(34U + (next_rng(g) % 28U));
            if (g->round > 2 && g->spawn_cooldown > 22U)
                g->spawn_cooldown -= 8U;
        }

        float gate_reach = 34.0f + (float)g->upg_wide_gate * 8.0f;

        for (int i = 0; i < PIRATE_MAX_ACTORS; ++i) {
            pirate_runner_t *a = &g->runners[i];
            if (!a->active) continue;
            if (a->dunk_timer) {
                --a->dunk_timer;
                a->x += a->vx;
                a->y += a->vy;
                a->vy += 0.55f;
                if (a->dunk_timer == 0U) a->active = false;
                continue;
            }
            a->x += a->vx;
            a->y += (k_lane_y[a->lane] - a->y) * 0.25f;

            /* Puzzle Switch Bridge at x = 232 (between Gate 0 and Gate 1):
             * diverts runners across connected lanes! */
            if (!a->switched_bridge && a->x >= 226.0f && a->x <= 238.0f) {
                a->switched_bridge = true;
                if (g->bridge_dir == 1U && (a->lane == 0U || a->lane == 1U))
                    a->lane = (uint8_t)(1U - a->lane);
                else if (g->bridge_dir == 2U && (a->lane == 1U || a->lane == 2U))
                    a->lane = (uint8_t)(3U - a->lane);
            }

            for (int c = 0; c < 2; ++c) {
                float gx = k_gate_x[c];
                bool near_x = fabsf(a->x - gx) <= 16.0f;
                bool blocked = fabsf(g->gate_y[c] - a->y) <= gate_reach;
                if (!near_x || !blocked) continue;

                if (a->kind == 1 && c == 0 && !a->jumped_col0) {
                    a->jumped_col0 = true;
                    a->lane = (uint8_t)((a->lane + 1U + (next_rng(g) & 1U)) % 3U);
                    a->x += 18.0f;
                    show_toast(g, 6U, a->x, a->y - 28.0f);
                    continue;
                }
                a->dunk_timer = 18U;
                a->vx = -1.8f;
                a->vy = -4.2f;
                if (a->kind == 2) {
                    g->combo = 0;
                    if (g->score >= 30U) g->score -= 30U;
                    show_toast(g, 4U, gx, a->y - 24.0f);
                } else {
                    ++g->combo;
                    add_score(g, 50U + (uint32_t)g->combo * 10U);
                    show_toast(g, 1U, gx, a->y - 24.0f);
                    if (g->quota_left) --g->quota_left;
                }
            }

            if (a->x >= 392.0f && !a->dunk_timer) {
                a->active = false;
                if (a->kind == 2) {
                    add_score(g, 80U);
                    show_toast(g, 2U, 388.0f, a->y - 24.0f);
                } else {
                    g->combo = 0;
                    if (g->hp > 0) --g->hp;
                    show_toast(g, 4U, 388.0f, a->y - 24.0f);
                    if (g->hp == 0U) {
                        g->stage = PIRATE_STAGE_GAME_OVER;
                        return;
                    }
                }
            }
        }
        if (g->quota_left == 0U) {
            g->stage = PIRATE_STAGE_CLEAR;
            g->next_stage = PIRATE_STAGE_FIND_HIDDEN;
            g->stage_timer = 36U;
            add_score(g, 150U);
        }
    } else if (g->stage == PIRATE_STAGE_FIND_HIDDEN) {
        for (int i = 0; i < PIRATE_MAX_SPOTS; ++i) {
            pirate_spot_t *s = &g->spots[i];
            if (s->caught) continue;
            if (s->shake_ticks) --s->shake_ticks;
            if (s->peek_timer) {
                --s->peek_timer;
                if (s->peek_timer == 0U) {
                    s->cooldown = (uint8_t)(22U + (next_rng(g) % 34U));
                    if (s->is_target && !s->auto_revealed && (next_rng(g) % 3U) == 0U) {
                        int other = (int)(next_rng(g) % PIRATE_MAX_SPOTS);
                        if (!g->spots[other].is_target && !g->spots[other].caught) {
                            uint8_t tmp_type = g->spots[other].suspect_type;
                            g->spots[other].suspect_type = s->suspect_type;
                            g->spots[other].is_target = true;
                            g->spots[other].shake_ticks = 14U;
                            s->suspect_type = tmp_type;
                            s->is_target = false;
                        }
                    }
                }
            } else if (s->cooldown) {
                --s->cooldown;
                if (s->cooldown == 4U && s->is_target)
                    s->shake_ticks = 12U;
                if (s->cooldown == 0U)
                    s->peek_timer = (uint8_t)(44U + (next_rng(g) % 34U));
            }
        }
        if (g->stage_timer) {
            --g->stage_timer;
            if (g->stage_timer == 0U && g->targets_found < g->targets_total) {
                if (g->hp > 0) --g->hp;
                if (g->hp == 0U) {
                    g->stage = PIRATE_STAGE_GAME_OVER;
                } else {
                    g->stage = PIRATE_STAGE_CLEAR;
                    g->next_stage = PIRATE_STAGE_TRAP_GRID;
                    g->stage_timer = 36U;
                }
            }
        }
    } else if (g->stage == PIRATE_STAGE_TRAP_GRID) {
        float tx, ty;
        grid_screen_pos(g->pirate_r, g->pirate_c, &tx, &ty);
        g->pirate_anim_x += (tx - g->pirate_anim_x) * 0.35f;
        g->pirate_anim_y += (ty - g->pirate_anim_y) * 0.35f;
    }
}

void pirate_game_pointer(pirate_game_t *g, float x, float y, bool pressed, bool fresh)
{
    if (!g) return;
    if (!pressed) {
        g->drag_gate = -1;
        return;
    }

    if (g->stage == PIRATE_STAGE_TITLE) {
        if (fresh) {
            g->hp = 3U;
            g->score = 0;
            g->round = 1U;
            g->upg_wide_gate = 0;
            g->upg_searchlight = 0;
            g->upg_extra_walls = 0;
            pirate_game_start_act(g, PIRATE_STAGE_SWIPE_BLOCK);
        }
        return;
    }
    if (g->stage == PIRATE_STAGE_GAME_OVER) {
        if (fresh) pirate_game_reset(g, g->rng + 17U);
        return;
    }
    if (g->stage == PIRATE_STAGE_CLEAR) {
        if (fresh && g->stage_timer < 28U)
            pirate_game_start_act(g, g->next_stage);
        return;
    }
    if (g->stage == PIRATE_STAGE_UPGRADE) {
        if (fresh) {
            /* Three vertical upgrade cards at y = 136..202, 214..280, 292..358 */
            for (unsigned i = 0; i < 3U; ++i) {
                float cy0 = 136.0f + (float)i * 78.0f;
                if (x >= 44.0f && x <= 436.0f && y >= cy0 && y <= cy0 + 68.0f) {
                    pirate_game_apply_upgrade(g, i);
                    return;
                }
            }
        }
        return;
    }

    if (g->stage == PIRATE_STAGE_SWIPE_BLOCK) {
        /* Tap center bridge switch (x=208..258) to toggle lane diversion! */
        if (fresh && x >= 206.0f && x <= 258.0f && y >= 100.0f && y <= 400.0f) {
            g->bridge_dir = (uint8_t)((g->bridge_dir + 1U) % 3U);
            return;
        }
        if (fresh) {
            g->drag_gate = (x < 232.0f) ? 0 : 1;
        }
        int col = g->drag_gate >= 0 ? g->drag_gate : ((x < 232.0f) ? 0 : 1);
        float cy = y;
        if (cy < 120.0f) cy = 120.0f;
        if (cy > 384.0f) cy = 384.0f;
        for (int l = 0; l < 3; ++l) {
            if (fabsf(cy - k_lane_y[l]) < 32.0f)
                cy = k_lane_y[l];
        }
        g->gate_target_y[col] = cy;
    } else if (g->stage == PIRATE_STAGE_FIND_HIDDEN && fresh) {
        int best = -1;
        float best_d2 = 46.0f * 46.0f;
        for (int i = 0; i < PIRATE_MAX_SPOTS; ++i) {
            if (g->spots[i].caught) continue;
            float dx = x - g->spots[i].x;
            float dy = y - (g->spots[i].y - 24.0f);
            float d2 = dx * dx + dy * dy;
            if (d2 < best_d2) {
                best_d2 = d2;
                best = i;
            }
        }
        if (best >= 0) {
            pirate_spot_t *s = &g->spots[best];
            s->shake_ticks = 14U;
            s->peek_timer = 38U;
            if (s->is_target) {
                s->caught = true;
                ++g->targets_found;
                ++g->combo;
                add_score(g, 100U + (uint32_t)g->combo * 25U);
                show_toast(g, 3U, s->x, s->y - 48.0f);
                if (g->targets_found >= g->targets_total) {
                    g->stage = PIRATE_STAGE_CLEAR;
                    g->next_stage = PIRATE_STAGE_TRAP_GRID;
                    g->stage_timer = 38U;
                    add_score(g, 200U);
                }
            } else {
                /* Tapped a villager or a lookalike pirate not matching the clue! */
                g->combo = 0;
                if (g->stage_timer > 45U) g->stage_timer -= 45U;
                show_toast(g, 4U, s->x, s->y - 48.0f);
            }
        }
    } else if (g->stage == PIRATE_STAGE_TRAP_GRID && fresh) {
        int best_r = -1, best_c = -1;
        float best_d2 = 28.0f * 28.0f;
        for (int r = 0; r < PIRATE_GRID_SIZE; ++r) {
            for (int c = 0; c < PIRATE_GRID_SIZE; ++c) {
                float sx, sy;
                grid_screen_pos(r, c, &sx, &sy);
                float dx = x - sx, dy = y - sy;
                if (dx * dx + dy * dy < best_d2) {
                    best_d2 = dx * dx + dy * dy;
                    best_r = r;
                    best_c = c;
                }
            }
        }
        if (best_r >= 0 && !is_cell_blocked(g->grid[best_r][best_c]) &&
            !(best_r == g->pirate_r && best_c == g->pirate_c)) {
            bool was_beacon = (g->grid[best_r][best_c] == 4U);
            g->grid[best_r][best_c] = (uint8_t)(1U + ((best_r + best_c) & 1));
            ++g->moves_used;

            /* Golden Beacon Combo: automatically sprouts a bonus diamond tree
             * right on the pirate's intended next step (or an adjacent tile)! */
            if (was_beacon) {
                int nr[6], nc[6];
                get_hex_neighbors(g->pirate_r, g->pirate_c, nr, nc);
                for (int i = 0; i < 6; ++i) {
                    int rr = nr[i], cc = nc[i];
                    if (rr >= 0 && rr < PIRATE_GRID_SIZE && cc >= 0 && cc < PIRATE_GRID_SIZE &&
                        !is_cell_blocked(g->grid[rr][cc]) && !(rr == g->pirate_r && cc == g->pirate_c)) {
                        g->grid[rr][cc] = 2U;
                        break;
                    }
                }
                add_score(g, 80U);
            }

            /* Pirate Boss Turn: every 3rd turn he charges a 2-step dash! */
            int steps = (g->dash_charge >= 2U) ? 2 : 1;
            g->dash_charge = (uint8_t)((g->dash_charge + 1U) % 3U);

            for (int s = 0; s < steps; ++s) {
                int next_r = g->pirate_r, next_c = g->pirate_c;
                if (!choose_pirate_escape_step(g, g->pirate_r, g->pirate_c, &next_r, &next_c)) {
                    add_score(g, 300U + (uint32_t)(g->moves_used < 12U ? (12U - g->moves_used) * 25U : 0U));
                    show_toast(g, 5U, g->pirate_anim_x, g->pirate_anim_y - 36.0f);
                    g->stage = PIRATE_STAGE_CLEAR;
                    g->next_stage = PIRATE_STAGE_UPGRADE;
                    g->stage_timer = 42U;
                    return;
                }
                g->pirate_r = (int8_t)next_r;
                g->pirate_c = (int8_t)next_c;
                if (next_r == 0 || next_r == PIRATE_GRID_SIZE - 1 ||
                    next_c == 0 || next_c == PIRATE_GRID_SIZE - 1) {
                    show_toast(g, 4U, g->pirate_anim_x, g->pirate_anim_y - 36.0f);
                    if (g->hp > 0) --g->hp;
                    if (g->hp == 0U) {
                        g->stage = PIRATE_STAGE_GAME_OVER;
                    } else {
                        g->stage = PIRATE_STAGE_CLEAR;
                        g->next_stage = PIRATE_STAGE_UPGRADE;
                        g->stage_timer = 42U;
                    }
                    return;
                }
            }
            update_escape_preview(g);
        }
    }
}

void pirate_game_action(pirate_game_t *g, int code, bool pressed)
{
    if (!g || !pressed) return;
    if (g->stage == PIRATE_STAGE_TITLE || g->stage == PIRATE_STAGE_GAME_OVER) {
        pirate_game_pointer(g, 240.0f, 240.0f, true, true);
        return;
    }
    if (g->stage == PIRATE_STAGE_UPGRADE) {
        pirate_game_apply_upgrade(g, (unsigned)code % 3U);
        return;
    }
    if (g->stage == PIRATE_STAGE_SWIPE_BLOCK) {
        if (code == 2) g->gate_target_y[0] = fmaxf(k_lane_y[0], g->gate_target_y[0] - 104.0f);
        else if (code == 5) g->gate_target_y[0] = fminf(k_lane_y[2], g->gate_target_y[0] + 104.0f);
        else if (code == 0) g->gate_target_y[1] = fmaxf(k_lane_y[0], g->gate_target_y[1] - 104.0f);
        else if (code == 1) g->gate_target_y[1] = fminf(k_lane_y[2], g->gate_target_y[1] + 104.0f);
        else if (code == 6) g->bridge_dir = (uint8_t)((g->bridge_dir + 1U) % 3U);
    }
}

void pirate_game_imu(pirate_game_t *g, float gx, float gy)
{
    (void)gy;
    if (!g) return;
    g->wind_x = fmaxf(-1.0f, fminf(1.0f, gx * 1.4f));
}

uint32_t pirate_game_state_hash(const pirate_game_t *g)
{
    if (!g) return 0;
    uint32_t h = 2166136261U;
    h = (h ^ (uint32_t)g->stage) * 16777619U;
    h = (h ^ g->tick) * 16777619U;
    h = (h ^ g->score) * 16777619U;
    h = (h ^ g->hp) * 16777619U;
    h = (h ^ g->round) * 16777619U;
    h = (h ^ g->upg_wide_gate) * 16777619U;
    h = (h ^ g->upg_searchlight) * 16777619U;
    h = (h ^ g->upg_extra_walls) * 16777619U;
    h = (h ^ g->rng) * 16777619U;
    return h;
}
