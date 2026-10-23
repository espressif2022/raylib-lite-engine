// SPDX-License-Identifier: Apache-2.0
#include "pirate_view.h"
#include <math.h>
#include "assets_ids.h"
#include "raylib_lite_design_view.h"

#define FIELD 480.0f
static raylib_lite_design_view_t s_view = {1.0f, 480.0f, 480.0f};
static Font s_zh = {0};

static const Color C_INK   = {38, 42, 46, 255};
static const Color C_PAPER = {250, 244, 228, 255};
static const Color C_RED   = {218, 78, 66, 255};
static const Color C_GREEN = {178, 218, 156, 255};
static const Color C_GOLD  = {244, 198, 74, 255};
static const Color C_WOOD  = {224, 194, 152, 255};
static const Color C_BLUE  = {88, 148, 204, 255};

static float zh_size(void)
{
    int base = s_zh.baseSize > 0 ? s_zh.baseSize : 18;
    if (raylib_lite_design_view_native(s_view) || s_view.scale <= 0.0f)
        return (float)base;
    return (float)base / s_view.scale;
}

static void draw_zh(const char *text, int x, int y, Color color)
{
    if (s_zh.texture.id) {
        DrawTextEx(s_zh, text, (Vector2){(float)x, (float)y}, zh_size(), 1.0f, color);
    } else {
        DrawText(text, x, y, raylib_lite_design_font(s_view, 14), color);
    }
}

static void draw_zh_centered(const char *text, int cx, int y, Color color)
{
    if (s_zh.texture.id) {
        float sz = zh_size();
        Vector2 m = MeasureTextEx(s_zh, text, sz, 1.0f);
        DrawTextEx(s_zh, text, (Vector2){(float)cx - m.x * 0.5f, (float)y}, sz, 1.0f, color);
    } else {
        int f = raylib_lite_design_font(s_view, 14);
        DrawText(text, cx - MeasureText(text, f) / 2, y, f, color);
    }
}

static void draw_spr(raylib_lite_atlas_t atlas, raylib_lite_asset_id_t id,
                     float cx, float bottom_y)
{
    const raylib_lite_sprite_frame_t *f = raylib_lite_atlas_get_frame(atlas, id);
    if (!f) return;
    float dx = floorf(cx - f->source.width * 0.5f + 0.5f);
    float dy = floorf(bottom_y - f->source.height + 0.5f);
    DrawTexturePro(atlas.texture, f->source,
                   (Rectangle){dx, dy, f->source.width, f->source.height},
                   (Vector2){0, 0}, 0.0f, WHITE);
}

static void draw_paper_box(int x, int y, int w, int h, Color fill)
{
    DrawRectangle(x - 2, y - 2, w + 4, h + 4, C_INK);
    DrawRectangle(x, y, w, h, fill);
}

static const char *modifier_label(pirate_modifier_t mod)
{
    switch (mod) {
        case PIRATE_MOD_TAILWIND:    return "变数:顺风加速";
        case PIRATE_MOD_JUMPER_RAID: return "变数:跳跳精锐";
        case PIRATE_MOD_MISTY_TIDE:  return "变数:浓雾涨潮";
        default:                     return "变数:晴朗微风";
    }
}

static const char *clue_label(pirate_clue_t clue)
{
    switch (clue) {
        case PIRATE_CLUE_PURPLE_BOMB: return "侦察线索:抓[紫色头巾+抱炸药桶]";
        case PIRATE_CLUE_STRAW_COIN:  return "侦察线索:抓[偷戴草帽+拿金币]";
        default:                      return "侦察线索:抓[黑帽+持弯刀]";
    }
}

static void draw_top_banner(const pirate_game_t *g, const char *act_title, const char *sub_line)
{
    draw_paper_box(16, 10, 448, 46, C_PAPER);
    draw_zh(act_title, 26, 14, C_INK);
    draw_zh(sub_line ? sub_line : modifier_label(g->modifier), 26, 34, C_RED);
    draw_zh(TextFormat("得分 %04lu", (unsigned long)g->score), 246, 14, C_INK);
    draw_zh(TextFormat("第%u轮", (unsigned)g->round), 246, 34, (Color){58, 132, 86, 255});
    draw_zh("生命", 362, 22, C_INK);
    for (unsigned i = 0; i < 3U; ++i) {
        int hx = 410 + (int)i * 18;
        DrawCircle(hx, 31, 7, C_INK);
        DrawCircle(hx, 31, 5, i < g->hp ? C_RED : (Color){210, 204, 192, 255});
    }
}

static void draw_bottom_hint(const char *hint)
{
    draw_paper_box(16, 434, 448, 34, C_PAPER);
    draw_zh_centered(hint, 240, 442, C_INK);
}

static void draw_act1_swipe_block(const pirate_game_t *g, raylib_lite_atlas_t spr)
{
    draw_top_banner(g, TextFormat("第一幕:堵海盗 剩余:%u", (unsigned)g->quota_left), NULL);

    draw_paper_box(-6, 86, 72, 332, (Color){208, 162, 118, 255});
    draw_paper_box(392, 82, 96, 340, C_GREEN);

    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_BASE, 440.0f, 218.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_TOP,  440.0f, 158.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_TREASURE,        438.0f, 326.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_TREE_DIAMOND,    438.0f, 412.0f);

    static const int lane_ys[3] = {152, 256, 360};
    for (int l = 0; l < 3; ++l) {
        int ly = lane_ys[l];
        const int segs[3][2] = {{66, 140}, {196, 268}, {324, 392}};
        for (int s = 0; s < 3; ++s) {
            int x0 = segs[s][0], x1 = segs[s][1];
            draw_paper_box(x0, ly - 12, x1 - x0, 24, C_WOOD);
            for (int px = x0 + 12; px < x1; px += 16)
                DrawRectangle(px, ly - 12, 2, 24, C_INK);
        }
    }

    /* Draw the interactive Puzzle Switch Bridge at x=232 between Gate 0 and Gate 1! */
    if (g->bridge_dir == 1U) {
        DrawLineEx((Vector2){222, 152}, (Vector2){242, 256}, 8.0f, C_INK);
        DrawLineEx((Vector2){222, 152}, (Vector2){242, 256}, 4.0f, C_GOLD);
    } else if (g->bridge_dir == 2U) {
        DrawLineEx((Vector2){222, 256}, (Vector2){242, 360}, 8.0f, C_INK);
        DrawLineEx((Vector2){222, 256}, (Vector2){242, 360}, 4.0f, C_GOLD);
    }
    draw_paper_box(214, 92, 36, 22, C_GOLD);
    draw_zh_centered("变道", 232, 95, C_INK);

    static const int gate_xs[2] = {168, 296};
    for (int c = 0; c < 2; ++c) {
        int gx = gate_xs[c];
        DrawRectangleLines(gx - 22, 88, 44, 326, (Color){118, 168, 192, 255});
        if (g->upg_wide_gate > 0) {
            int pad = (int)g->upg_wide_gate * 8;
            draw_paper_box(gx - 18, (int)g->gate_y[c] - 30 - pad, 36, 60 + pad * 2, C_GOLD);
        }
        draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_GATE_BARRICADE, (float)gx, g->gate_y[c] + 30.0f);
    }

    for (int i = 0; i < PIRATE_MAX_ACTORS; ++i) {
        const pirate_runner_t *a = &g->runners[i];
        if (!a->active) continue;
        raylib_lite_asset_id_t id = a->kind == 2 ? RAYLIB_LITE_ASSET_ID_SPR_VILLAGER_POSTMAN
                                  : a->kind == 1 ? RAYLIB_LITE_ASSET_ID_SPR_PIRATE_JUMPER
                                                 : RAYLIB_LITE_ASSET_ID_SPR_PIRATE_SCOUT;
        float bob = a->dunk_timer ? 0.0f : fabsf(sinf((float)g->tick * 0.28f + (float)i)) * 4.0f;
        draw_spr(spr, id, a->x, a->y + 12.0f - bob);
        if (a->dunk_timer) {
            int r = (int)(20U - a->dunk_timer) * 2;
            DrawCircleLines((int)a->x, (int)a->y + 10, (float)r, C_INK);
        }
    }

    draw_bottom_hint("上下滑动红木闸:把海盗弹进水里,放行草帽村民!");
}

static void draw_act2_find_hidden(const pirate_game_t *g, raylib_lite_atlas_t spr)
{
    unsigned secs = (g->stage_timer + 29U) / 30U;
    draw_top_banner(g, TextFormat("第二幕:找海盗 %u/%u  %us",
                                  (unsigned)g->targets_found, (unsigned)g->targets_total, secs),
                    clue_label(g->clue_rule));

    draw_paper_box(-10, 232, 500, 198, C_GREEN);
    draw_paper_box(-10, 324, 500, 110, (Color){162, 208, 142, 255});

    static const raylib_lite_asset_id_t k_suspect_ids[5] = {
        RAYLIB_LITE_ASSET_ID_SPR_PIRATE_SCOUT,     /* 0: black hat + sword */
        RAYLIB_LITE_ASSET_ID_SPR_PIRATE_JUMPER,    /* 1: purple bandana + bomb */
        RAYLIB_LITE_ASSET_ID_SPR_PIRATE_DISGUISED, /* 2: straw hat + coin */
        RAYLIB_LITE_ASSET_ID_SPR_VILLAGER_POSTMAN, /* 3: villager + letter */
        RAYLIB_LITE_ASSET_ID_SPR_VILLAGER_BLUE,    /* 4: villager blue */
    };

    for (int i = 0; i < PIRATE_MAX_SPOTS; ++i) {
        const pirate_spot_t *s = &g->spots[i];
        float sx = s->x + (s->shake_ticks ? ((s->shake_ticks & 1U) ? 3.0f : -3.0f) : 0.0f);
        float sy = s->y;

        if (s->auto_revealed && !s->caught) {
            DrawCircle((int)sx, (int)sy - 28, 30.0f, (Color){255, 236, 132, 255});
            DrawCircleLines((int)sx, (int)sy - 28, 30.0f, C_GOLD);
        }

        if (s->caught || s->peek_timer > 0 || s->auto_revealed) {
            uint8_t st = s->suspect_type < 5U ? s->suspect_type : 0U;
            float pop_y = (s->cover_kind <= 2) ? (sy - 20.0f) : sy;
            draw_spr(spr, k_suspect_ids[st], sx, pop_y);
        }

        if (s->cover_kind == 0) {
            draw_spr(spr, (i & 1) ? RAYLIB_LITE_ASSET_ID_SPR_COTTAGE_CORAL
                                  : RAYLIB_LITE_ASSET_ID_SPR_COTTAGE_BLUE, sx, sy + 14.0f);
        } else if (s->cover_kind == 1) {
            draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_TREE_DIAMOND, sx, sy + 14.0f);
        } else if (s->cover_kind == 2) {
            draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_BUSH_HIDEOUT, sx, sy + 14.0f);
        } else {
            draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_BASE, sx, sy + 14.0f);
            draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_TOP, sx, sy - 44.0f);
        }

        if (s->caught) {
            DrawCircleLines((int)sx, (int)sy - 28, 28.0f, C_RED);
            DrawCircleLines((int)sx, (int)sy - 28, 27.0f, C_RED);
            draw_zh_centered("抓获!", (int)sx, (int)sy - 66, C_RED);
        }
    }

    draw_bottom_hint("按顶部侦察线索揪出真凶! 别误点其他伪装者!");
}

static void draw_act3_trap_grid(const pirate_game_t *g, raylib_lite_atlas_t spr)
{
    const char *dash_sub = (g->dash_charge >= 2U)
        ? "预警:海盗下回合将连跨2步冲刺! 快堵红圈!"
        : TextFormat("蓄力:%u/2  点击金石碑可连造两棵树!", (unsigned)g->dash_charge);
    draw_top_banner(g, TextFormat("第三幕:围海盗 步数:%u", (unsigned)g->moves_used), dash_sub);

    for (int r = 0; r < PIRATE_GRID_SIZE; ++r) {
        for (int c = 0; c < PIRATE_GRID_SIZE; ++c) {
            int cx = 84 + c * 50 + ((r & 1) ? 24 : 0);
            int cy = 114 + r * 46;
            bool is_edge = (r == 0 || r == PIRATE_GRID_SIZE - 1 || c == 0 || c == PIRATE_GRID_SIZE - 1);
            uint8_t cell = g->grid[r][c];
            Color stone = (cell == 4U) ? C_GOLD
                        : is_edge ? (Color){238, 224, 192, 255} : C_GREEN;
            DrawEllipse(cx, cy, 22.0f, 15.0f, C_INK);
            DrawEllipse(cx, cy, 19.0f, 12.0f, stone);

            /* Highlight the pirate's intended next escape stone so the player
             * can outsmart the BFS AI tactically! */
            if (r == g->escape_next_r && c == g->escape_next_c && cell == 0) {
                DrawEllipseLines(cx, cy, 23.0f, 16.0f, C_RED);
                if (g->dash_charge >= 2U)
                    DrawEllipseLines(cx, cy, 25.0f, 18.0f, C_RED);
            }

            if (cell == 1) {
                draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_COTTAGE_CORAL, (float)cx, (float)cy + 14.0f);
            } else if (cell == 2) {
                draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_TREE_DIAMOND, (float)cx, (float)cy + 14.0f);
            } else if (cell == 3) {
                draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_BARREL_OBSTACLE, (float)cx, (float)cy + 14.0f);
            } else if (cell == 4) {
                DrawCircle(cx, cy - 2, 5, C_INK);
                DrawCircle(cx, cy - 2, 3, C_PAPER);
            }
        }
    }

    float bob = fabsf(sinf((float)g->tick * 0.22f)) * 4.0f;
    raylib_lite_asset_id_t pid = (g->dash_charge >= 2U)
        ? RAYLIB_LITE_ASSET_ID_SPR_PIRATE_JUMPER
        : RAYLIB_LITE_ASSET_ID_SPR_PIRATE_SCOUT;
    draw_spr(spr, pid, g->pirate_anim_x, g->pirate_anim_y + 10.0f - bob);

    draw_bottom_hint("点击圆石种树建房,别让小海盗逃到岛边缘!");
}

static void draw_upgrade_workshop(const pirate_game_t *g, raylib_lite_atlas_t spr)
{
    draw_top_banner(g, TextFormat("第%u轮完成! 海岛工坊", (unsigned)g->round),
                    "选择一项科技升级 进入下一轮挑战");

    static const char *const k_upg_title[4] = {
        "加宽木闸 (第一幕)",
        "鹰眼探照灯 (第二幕)",
        "预筑路障 (第三幕)",
        "加固宝库 (立即生效)",
    };
    static const char *const k_upg_desc[4] = {
        "木闸撞击范围+8px,更易弹飞海盗",
        "自动揭露1名真凶位置并延长4秒",
        "开局在圆石棋盘上额外多放2座小屋",
        "立刻恢复1点生命并奖励+250得分",
    };
    static const raylib_lite_asset_id_t k_upg_icon[4] = {
        RAYLIB_LITE_ASSET_ID_SPR_GATE_BARRICADE,
        RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_TOP,
        RAYLIB_LITE_ASSET_ID_SPR_COTTAGE_BLUE,
        RAYLIB_LITE_ASSET_ID_SPR_TREASURE,
    };

    for (unsigned i = 0; i < 3U; ++i) {
        uint8_t id = g->upgrade_choices[i] & 3U;
        int cy0 = 136 + (int)i * 78;
        draw_paper_box(44, cy0, 392, 66, C_PAPER);
        DrawRectangle(50, cy0 + 6, 6, 54, i == 0 ? C_GOLD : (i == 1 ? C_BLUE : C_RED));
        draw_spr(spr, k_upg_icon[id], 94.0f, (float)(cy0 + 62));
        draw_zh(k_upg_title[id], 136, cy0 + 11, C_INK);
        draw_zh(k_upg_desc[id],  136, cy0 + 36, C_RED);
    }

    draw_bottom_hint("点击任意一张手绘科技卡 升级你的灯塔小岛!");
}

static void draw_title_or_overlay(const pirate_game_t *g, raylib_lite_atlas_t spr)
{
    draw_paper_box(-10, 300, 500, 185, C_GREEN);
    draw_paper_box(-10, 408, 500, 75, C_PAPER);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_COTTAGE_BLUE,     102.0f, 336.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_TREE_DIAMOND,     236.0f, 344.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_BASE,  382.0f, 316.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_LIGHTHOUSE_TOP,   382.0f, 256.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_VILLAGER_POSTMAN, 172.0f, 348.0f);
    draw_spr(spr, RAYLIB_LITE_ASSET_ID_SPR_PIRATE_SCOUT,     308.0f, 348.0f);

    draw_paper_box(56, 88, 368, 246, C_PAPER);
    DrawRectangle(64, 96, 352, 6, C_GOLD);

    if (g->stage == PIRATE_STAGE_TITLE) {
        draw_zh_centered("灯塔小岛:海盗保卫战", 240, 114, C_INK);
        draw_zh_centered("手绘日常 x 益智三幕闯关 x 科技升级", 240, 144, C_RED);
        draw_zh_centered("1. 划动木闸+变道开关  2. 线索推理抓海盗", 240, 178, C_INK);
        draw_zh_centered("3. 预判红圈+金石碑连击 围困海盗头目", 240, 204, C_INK);
        draw_zh_centered(TextFormat("最高纪录: %04lu", (unsigned long)g->high_score), 240, 232, (Color){58, 132, 86, 255});
        draw_paper_box(136, 266, 208, 44, C_GOLD);
        draw_zh_centered("点击屏幕 开始巡逻!", 240, 279, C_INK);
    } else if (g->stage == PIRATE_STAGE_GAME_OVER) {
        draw_zh_centered("宝藏失守! 巡逻结束", 240, 122, C_RED);
        draw_zh_centered(TextFormat("最终得分: %lu   守卫轮数: %u",
                                    (unsigned long)g->score, (unsigned)g->round), 240, 168, C_INK);
        draw_zh_centered(TextFormat("最高纪录: %lu", (unsigned long)g->high_score), 240, 204, (Color){58, 132, 86, 255});
        draw_paper_box(136, 258, 208, 44, C_GOLD);
        draw_zh_centered("点击屏幕 重新挑战!", 240, 271, C_INK);
    }
}

void pirate_view_render(const pirate_game_t *g, raylib_lite_atlas_t bg_atlas,
                        raylib_lite_atlas_t spr_atlas, Font zh_font)
{
    if (!g) return;
    s_zh = zh_font;
    BeginDrawing();
    s_view = raylib_lite_design_view(FIELD);
    raylib_lite_design_field_begin(s_view, FIELD, FIELD);

    const raylib_lite_sprite_frame_t *bg =
        raylib_lite_atlas_get_frame(bg_atlas, RAYLIB_LITE_ASSET_ID_STORY_PAPER_BG);
    if (bg) {
        DrawTexturePro(bg_atlas.texture, bg->source,
                       (Rectangle){0, 0, FIELD, FIELD}, (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground((Color){212, 234, 244, 255});
    }

    float cloud_shift = fmodf((float)g->tick * 0.35f, 540.0f);
    draw_spr(spr_atlas, RAYLIB_LITE_ASSET_ID_SPR_SUN, 424.0f, 118.0f);
    draw_spr(spr_atlas, RAYLIB_LITE_ASSET_ID_SPR_CLOUD, 480.0f - cloud_shift, 112.0f);
    draw_spr(spr_atlas, RAYLIB_LITE_ASSET_ID_SPR_CLOUD, fmodf(240.0f + cloud_shift, 520.0f) - 20.0f, 128.0f);

    if (g->stage == PIRATE_STAGE_TITLE || g->stage == PIRATE_STAGE_GAME_OVER) {
        draw_title_or_overlay(g, spr_atlas);
    } else if (g->stage == PIRATE_STAGE_SWIPE_BLOCK) {
        draw_act1_swipe_block(g, spr_atlas);
    } else if (g->stage == PIRATE_STAGE_FIND_HIDDEN) {
        draw_act2_find_hidden(g, spr_atlas);
    } else if (g->stage == PIRATE_STAGE_TRAP_GRID) {
        draw_act3_trap_grid(g, spr_atlas);
    } else if (g->stage == PIRATE_STAGE_UPGRADE) {
        draw_upgrade_workshop(g, spr_atlas);
    } else if (g->stage == PIRATE_STAGE_CLEAR) {
        draw_act3_trap_grid(g, spr_atlas);
        draw_paper_box(96, 176, 288, 112, C_PAPER);
        draw_zh_centered("本幕完美守卫! +200", 240, 198, (Color){58, 142, 88, 255});
        draw_zh_centered("点击进入下一幕挑战...", 240, 238, C_INK);
    }

    if (g->toast_ticks && g->toast_kind) {
        static const char *const k_toasts[] = {
            "", "落水! +50", "村民通过! +80", "抓获! +100", "哎呀!", "成功围困! +300", "跳越换道!"
        };
        const char *msg = k_toasts[g->toast_kind < 7 ? g->toast_kind : 0];
        Color col = (g->toast_kind == 4 || g->toast_kind == 6) ? C_RED : (Color){44, 138, 84, 255};
        int tx = (int)fmaxf(64.0f, fminf(416.0f, g->toast_x));
        int ty = (int)fmaxf(68.0f, fminf(396.0f, g->toast_y - (float)(26U - g->toast_ticks)));
        draw_paper_box(tx - 54, ty - 4, 108, 26, C_PAPER);
        draw_zh_centered(msg, tx, ty, col);
    }

    raylib_lite_design_field_end(s_view, FIELD, FIELD);
    EndDrawing();
}
