// SPDX-License-Identifier: Apache-2.0
#include "tower_view.h"

#include <math.h>
#include <stdio.h>
#include "assets_ids.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_design_view.h"
#include "raylib_lite_raylib.h"

static const Color C_BG = {5, 10, 20, 255};
static const Color C_GRASS = {12, 30, 35, 255};
static const Color C_GOLD = {255, 193, 61, 255};
static const Color C_CYAN = {42, 224, 231, 255};
static const Color C_RED = {255, 73, 105, 255};
static const Color C_BLUE = {110, 145, 255, 255};
static const float TOWER_RANGE[TOWER_TYPE_COUNT] = {96.0f, 78.0f, 112.0f};

static void draw_sprite(raylib_lite_atlas_t atlas, const char *name, float x, float y,
                        float size, float rotation, Color tint)
{
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(atlas, raylib_lite_asset_id(name));
    if (!frame) return;
    DrawTexturePro(atlas.texture, frame->source,
        (Rectangle){x, y, size, size}, (Vector2){size * .5f, size * .5f}, rotation, tint);
}

static void draw_sprite_id(raylib_lite_atlas_t atlas, raylib_lite_asset_id_t id, float x, float y,
                           float size, Color tint)
{
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(atlas, id);
    if (frame) DrawTexturePro(atlas.texture, frame->source,
        (Rectangle){x, y, size, size}, (Vector2){size * .5f, size * .5f}, 0, tint);
}

/* The 480x480 arena is centred in the design space; wider panels get wings. */
#define FIELD 480.0f
static raylib_lite_design_view_t s_view = {1.0f, 480.0f, 480.0f};
/* Text is sized for the camera it is drawn under (the map may be zoomed). */
static raylib_lite_design_view_t s_font = {1.0f, 480.0f, 480.0f};

/* Landscape panels zoom the 480x320 map to the full width and fold the HUD
 * and shop into one top bar; field (x, y) maps to (mx + x*k, my + (y-69)*k). */
typedef struct {
    bool wide;
    float k, mx, my;
} tower_layout_t;

static tower_layout_t layout_for(raylib_lite_design_view_t view)
{
    if (view.width < 600.0f || view.height > view.width) return (tower_layout_t){.k = 1.0f};
    float k = fminf(view.width / 480.0f, (view.height - 48.0f) / 320.0f);
    return (tower_layout_t){true, k, (view.width - 480.0f * k) * .5f, view.height - 320.0f * k};
}

/* Bar buttons 0-2 are the shop, 3 is pause. */
static Rectangle bar_button(raylib_lite_design_view_t view, tower_layout_t layout, int index)
{
    const float h = layout.my - 11.0f;
    if (index == 3) return (Rectangle){view.width - 50.0f, 4, 44, h};
    return (Rectangle){view.width - 54.0f - 3 * 87.0f + index * 87.0f, 4, 84, h};
}

static int font(int size) { return raylib_lite_design_font(s_font, size); }
/* Scaled panels keep the primary labels and drop secondary ones. */
static bool compact(void) { return !raylib_lite_design_view_native(s_view); }
static void draw_text(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x, y, font(size), color);
}
static int measure_text(const char *text, int size) { return MeasureText(text, font(size)); }

static void draw_wings(uint32_t tick)
{
    const int margin = (int)((s_view.width - FIELD) * .5f), height = (int)s_view.height;
    if (margin < 8) return;
    const int right = (int)s_view.width - margin;
    DrawRectangle(0, 0, margin, height, C_BG);
    DrawRectangle(right, 0, margin, height, C_BG);
    DrawRectangle(margin - 3, 0, 3, height, (Color){13, 72, 83, 255});
    DrawRectangle(right, 0, 3, height, (Color){13, 72, 83, 255});
    for (int i = 0; i < 6; ++i) {
        int y = 40 + i * (height - 80) / 5;
        int lit = (int)((tick / 20U + (unsigned)i) % 6U) == 0;
        Color trace = lit ? C_CYAN : (Color){20, 52, 60, 255};
        DrawRectangle(8, y, margin - 20, 2, trace);
        DrawRectangle(margin - 14, y - 6, 2, 8, trace);
        DrawRectangle(right + 12, y, margin - 20, 2, trace);
        DrawRectangle(right + 12, y, 2, 8, trace);
    }
}

static void draw_centered(const char *text, int y, int size, Color color)
{
    draw_text(text, (480 - measure_text(text, size)) / 2, y, size, color);
}

static void draw_pad(const tower_view_t *view, const tower_slot_t *tower)
{
    int x = tower->x, y = tower->y;
    Color main = tower->type == TOWER_PULSE ? C_CYAN
        : tower->type == TOWER_RAPID ? C_GOLD : C_BLUE;
    draw_sprite(view->atlas, "build_pad", x, y, 54, 0,
                tower->occupied ? (Color){150, 150, 150, 255} : WHITE);
    if (!tower->occupied) {
        Color plus = (view->game->tick / 15U) % 2 ? C_CYAN : (Color){76, 139, 139, 255};
        DrawRectangle(x - 8, y - 2, 16, 4, plus);
        DrawRectangle(x - 2, y - 8, 4, 16, plus);
        return;
    }
    const char *names[] = {"tower_pulse", "tower_rapid", "tower_frost"};
    draw_sprite(view->atlas, names[tower->type], x, y - 4, 58, 0, WHITE);
    DrawRectangle(x - 13, y + 14, 26, 7, (Color){4, 15, 21, 255});
    for (int i = 0; i < 3; ++i) DrawRectangle(x - 10 + i * 7, y + 16, 5, 3,
        i < tower->level ? main : (Color){39, 67, 70, 255});
}

static void draw_enemy(raylib_lite_atlas_t atlas, const tower_enemy_t *enemy)
{
    int x = (int)enemy->x, y = (int)enemy->y;
    const char *names[] = {"enemy_drone", "enemy_brute", "enemy_scout"};
    int size = enemy->kind == 1 ? 48 : enemy->kind == 2 ? 34 : 40;
    draw_sprite(atlas, names[enemy->kind], x, y, size, 0,
                enemy->slow_ticks ? (Color){145, 205, 255, 255} : WHITE);
    int hpw = (int)((enemy->hp / enemy->max_hp) * 28.0f);
    DrawRectangle(x - 15, y - size / 2 - 9, 30, 5, (Color){5, 13, 18, 255});
    DrawRectangle(x - 14, y - size / 2 - 8, hpw, 3,
        enemy->hp / enemy->max_hp > 0.35f ? (Color){73, 236, 135, 255} : C_RED);
}

static void draw_projectiles(const tower_view_t *view)
{
    for (size_t i = 0; i < TOWER_MAX_PROJECTILES; ++i) {
        const tower_projectile_t *projectile = &view->game->projectiles[i];
        if (!projectile->active) continue;
        const char *names[] = {"projectile_pulse", "projectile_rapid", "projectile_frost"};
        draw_sprite(view->atlas, names[projectile->kind], projectile->x, projectile->y,
                    projectile->kind == TOWER_RAPID ? 14 : 18, 0, WHITE);
    }
}

static void draw_effects(const tower_view_t *view)
{
    for (size_t i = 0; i < view->effect_count; ++i) {
        const tower_effect_t *effect = &view->effects[i];
        if (!effect->active) continue;
        raylib_lite_asset_id_t frame = raylib_lite_animation_frame_at(
            MOSAICO_ANIMATION_EXPLOSION_FRAMES,
            MOSAICO_ANIMATION_EXPLOSION_FRAME_COUNT,
            MOSAICO_ANIMATION_EXPLOSION_FRAME_TICKS, 12U - effect->ticks, false);
        draw_sprite_id(view->atlas, frame, effect->x, effect->y,
                       54 + (12 - effect->ticks) * 2, WHITE);
    }
}

static void draw_core(const tower_view_t *view)
{
    float pulse = 62 + (float)((view->game->tick / 5U) % 4);
    draw_sprite(view->atlas, "reactor_core", 467, 354, pulse, 0,
                view->game->base_hp > 6 ? WHITE : (Color){255, 110, 110, 255});
}

static void draw_hud(const tower_game_t *game)
{
    DrawRectangle(0, 0, 480, 69, C_BG);
    DrawRectangle(0, 65, 480, 4, (Color){13, 72, 83, 255});
    DrawRectangle(0, 65, 110, 2, C_CYAN);
    DrawRectangle(12, 9, 118, 47, (Color){10, 27, 39, 255});
    DrawRectangleLines(12, 9, 118, 47, (Color){35, 100, 112, 255});
    DrawRectangle(18, 15, 4, 35, C_CYAN);
    draw_text(TextFormat("WAVE %02u", game->wave), 29, 14, 17, RAYWHITE);
    draw_text(TextFormat("%02u/%02u  C%03u", game->wave_spawned, game->wave_total,
                        game->credits), 29, 36, 11, C_GOLD);
    DrawRectangle(140, 9, 132, 47, (Color){10, 27, 39, 255});
    DrawRectangleLines(140, 9, 132, 47, (Color){35, 77, 91, 255});
    draw_text(compact() ? "SCORE" : "TACTICAL SCORE", 151, 15, 11, (Color){107, 151, 160, 255});
    draw_text(TextFormat("%06lu", (unsigned long)game->score), 151, 34, 17, RAYWHITE);
    if (game->synergy_hits && !compact())
        draw_text(TextFormat("SYNC %lu", (unsigned long)game->synergy_hits),
                 211, 49, 7, C_BLUE);
    DrawRectangle(282, 9, 125, 47, (Color){10, 27, 39, 255});
    DrawRectangleLines(282, 9, 125, 47, (Color){35, 77, 91, 255});
    draw_text(TextFormat("CORE %02u", game->base_hp), 293, 15, 14,
        game->base_hp > 6 ? (Color){102, 233, 139, 255} : C_RED);
    DrawRectangle(293, 39, 102, 7, (Color){28, 48, 53, 255});
    DrawRectangle(295, 41, game->base_hp * 5, 3,
        game->base_hp > 6 ? (Color){78, 228, 134, 255} : C_RED);
    DrawRectangle(418, 9, 50, 47, (Color){13, 36, 47, 255});
    DrawRectangleLines(418, 9, 50, 47, C_CYAN);
    DrawRectangle(432, 21, 7, 22, (Color){181, 246, 245, 255});
    DrawRectangle(447, 21, 7, 22, (Color){181, 246, 245, 255});
}

static void draw_selected_range(const tower_game_t *game)
{
    if (game->selected_pad < 0 || game->selected_pad >= TOWER_PAD_COUNT) return;
    const tower_slot_t *tower = &game->towers[(unsigned)game->selected_pad];
    unsigned type = tower->occupied ? tower->type : game->selected_type;
    unsigned level = tower->occupied ? tower->level : 1;
    Color color = type == TOWER_PULSE ? C_CYAN
        : type == TOWER_RAPID ? C_GOLD : C_BLUE;
    float range = TOWER_RANGE[type] + (level - 1U) * 12.0f;
    DrawCircleLines(tower->x, tower->y, range, (Color){color.r, color.g, color.b, 150});
    const char *label = TextFormat("L%u  R%03u", level, (unsigned)range);
    if (!compact()) {
        DrawRectangle(tower->x - 34, tower->y - 39, 68, 16, (Color){3, 12, 18, 225});
        draw_text(label, tower->x - 29, tower->y - 36, 9, color);
        return;
    }
    int width = measure_text(label, 9) + 10, height = font(9) + 6;
    DrawRectangle(tower->x - width / 2, tower->y - 23 - height, width, height,
                  (Color){3, 12, 18, 225});
    draw_text(label, tower->x - width / 2 + 5, tower->y - 20 - height, 9, color);
}

static void draw_shop_card(const tower_game_t *game, int type, int x, const char *name,
                           const char *role, const char *cost, Color color)
{
    bool selected = game->selected_type == type;
    DrawRectangle(x + 3, 405, 143, 67, (Color){2, 8, 14, 255});
    DrawRectangle(x, 402, 146, 67, selected ? (Color){19, 47, 56, 255} : (Color){10, 25, 32, 255});
    DrawRectangleLines(x, 402, 146, 67, selected ? color : (Color){39, 72, 78, 255});
    DrawRectangle(x, 402, selected ? 45 : 18, 3, color);
    DrawRectangle(x + 8, 411, 35, 42, (Color){5, 16, 24, 255});
    DrawRectangleLines(x + 8, 411, 35, 42, (Color){44, 81, 88, 255});
    DrawRectangle(x + 13, 419, 25, 25, color);
    if (type == TOWER_PULSE) DrawRectangle(x + 23, 413, 5, 24, RAYWHITE);
    else if (type == TOWER_RAPID) {
        DrawRectangle(x + 17, 413, 5, 24, RAYWHITE);
        DrawRectangle(x + 29, 413, 5, 24, RAYWHITE);
    } else DrawTriangle((Vector2){x + 25, 411}, (Vector2){x + 14, 442},
                        (Vector2){x + 36, 442}, RAYWHITE);
    draw_text(name, x + 50, 411, 14, RAYWHITE);
    if (!compact()) draw_text(role, x + 50, 431, 10, (Color){101, 148, 155, 255});
    if (compact()) draw_text(cost, x + 138 - measure_text(cost, 13), 445, 13, C_GOLD);
    else draw_text(cost, x + 96, 449, 13, C_GOLD);
    if (selected) {
        DrawRectangle(x + 7, 460, 62, 2, color);
        if (!compact()) draw_text("READY", x + 13, 449, 10, color);
    }
}

static void draw_shop(const tower_game_t *game)
{
    DrawRectangle(0, 392, 480, 88, (Color){3, 10, 16, 255});
    DrawRectangle(0, 392, 480, 2, (Color){31, 106, 112, 255});
    draw_shop_card(game, TOWER_PULSE, 8, "PULSE", "2X FROZEN", "$70", C_CYAN);
    draw_shop_card(game, TOWER_RAPID, 164, "RAPID", "+2 FROZEN", "$95", C_GOLD);
    draw_shop_card(game, TOWER_FROST, 320, "FROST", "SETUP SLOW", "$120", C_BLUE);
}

static void draw_feedback(const tower_game_t *game)
{
    if (!game->feedback_ticks) return;
    const char *message = NULL;
    Color color = C_CYAN;
    switch ((tower_feedback_t)game->feedback) {
    case TOWER_FEEDBACK_BUILT: message = TextFormat("TOWER BUILT  -$%u", game->feedback_value); break;
    case TOWER_FEEDBACK_UPGRADED: message = TextFormat("UPGRADED  -$%u", game->feedback_value); break;
    case TOWER_FEEDBACK_REPLACED: message = TextFormat("TOWER CHANGED  -$%u", game->feedback_value); break;
    case TOWER_FEEDBACK_MAX_LEVEL: message = "MAX LEVEL"; color = C_GOLD; break;
    case TOWER_FEEDBACK_NO_CREDITS: message = TextFormat("NEED $%u", game->feedback_value); color = C_RED; break;
    default: return;
    }
    int width = measure_text(message, 12) + 20;
    DrawRectangle((480 - width) / 2, 372, width, 20, (Color){3, 12, 18, 235});
    DrawRectangleLines((480 - width) / 2, 372, width, 20, color);
    draw_centered(message, 376, 12, color);
}

static void draw_overlay(const char *title, const char *line, const char *action, Color accent)
{
    DrawRectangle(29, 125, 422, 204, (Color){1, 7, 13, 210});
    DrawRectangle(35, 119, 410, 202, (Color){5, 16, 27, 246});
    DrawRectangleLines(35, 119, 410, 202, (Color){31, 81, 92, 255});
    DrawRectangle(35, 119, 410, 5, accent);
    DrawRectangle(35, 119, 36, 12, accent);
    DrawRectangle(409, 119, 36, 12, accent);
    if (!compact())
        draw_text("MOSAICO // DEFENSE PROTOCOL", 52, 140, 11, (Color){94, 142, 153, 255});
    draw_centered(title, 166, 32, RAYWHITE);
    DrawRectangle(103, 207, 274, 2, (Color){30, 78, 88, 255});
    draw_centered(line, 224, 16, (Color){171, 211, 211, 255});
    DrawRectangle(107, 268, 266, 43, (Color){2, 11, 18, 255});
    DrawRectangle(111, 264, 258, 43, accent);
    DrawRectangle(118, 271, 244, 29, (Color){8, 27, 34, 255});
    draw_centered(action, 279, 15, RAYWHITE);
}

bool tower_view_apply_map(tower_game_t *game, raylib_lite_tilemap_t map)
{
    (void)game;
    const raylib_lite_renderer_vec2_t *points = NULL;
    size_t count = raylib_lite_tilemap_path_points(map, &points);
    tower_level_t level = {.path_count = (uint8_t)count};
    for (size_t i = 0; i < count && i < TOWER_MAX_PATH_POINTS; ++i) {
        level.path[i][0] = points[i].x;
        level.path[i][1] = points[i].y;
    }
    for (unsigned i = 0; i < TOWER_PAD_COUNT; ++i) {
        char name[12];
        snprintf(name, sizeof(name), "pad_%u", i);
        raylib_lite_map_object_t object = {0};
        if (raylib_lite_tilemap_find_object(map, raylib_lite_asset_id(name), &object)) {
            level.pads[i][0] = object.x;
            level.pads[i][1] = object.y;
        }
    }
    return tower_game_configure_level(&level);
}

void tower_view_add_explosion(tower_effect_t *effects, size_t count, float x, float y)
{
    for (size_t i = 0; i < count; ++i) {
        if (effects[i].active) continue;
        effects[i] = (tower_effect_t){.x = x, .y = y, .ticks = 12, .active = true};
        return;
    }
}

void tower_view_tick_effects(tower_effect_t *effects, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (effects[i].active && !--effects[i].ticks) effects[i].active = false;
    }
}

static void draw_bar(const tower_game_t *game, tower_layout_t layout)
{
    const int w = (int)(s_view.width + .5f), bar = (int)(layout.my + .5f);
    const int row1 = 5, row2 = bar - 28;
    DrawRectangle(0, 0, w, bar, C_BG);
    DrawRectangle(0, bar - 3, w, 3, (Color){13, 72, 83, 255});
    DrawRectangle(0, bar - 3, 110, 2, C_CYAN);
    DrawRectangle(10, 5, 4, bar - 13, C_CYAN);
    const char *wave = TextFormat("WAVE %02u", game->wave);
    draw_text(wave, 20, row1, 14, RAYWHITE);
    int width = measure_text(wave, 14);
    const char *count = TextFormat("%02u/%02u C%03u", game->wave_spawned, game->wave_total,
                                   game->credits);
    draw_text(count, 20, row2, 11, C_GOLD);
    if (measure_text(count, 11) > width) width = measure_text(count, 11);
    int x = 20 + width + 10;
    const char *score = TextFormat("%06lu", (unsigned long)game->score);
    draw_text("SCORE", x, row1, 11, (Color){107, 151, 160, 255});
    draw_text(score, x, row2, 14, RAYWHITE);
    x += measure_text(score, 14) + 10;
    const Color core = game->base_hp > 6 ? (Color){102, 233, 139, 255} : C_RED;
    const char *core_text = TextFormat("CORE %02u", game->base_hp);
    draw_text(core_text, x, row1, 11, core);
    const int bar_w = measure_text(core_text, 11);
    DrawRectangle(x, row2 + 6, bar_w, 7, (Color){28, 48, 53, 255});
    DrawRectangle(x + 2, row2 + 8, (bar_w - 4) * game->base_hp / 20, 3, core);
    static const char *names[TOWER_TYPE_COUNT] = {"PULSE", "RAPID", "FROST"};
    static const char *costs[TOWER_TYPE_COUNT] = {"$70", "$95", "$120"};
    const Color colors[TOWER_TYPE_COUNT] = {C_CYAN, C_GOLD, C_BLUE};
    for (int i = 0; i < TOWER_TYPE_COUNT; ++i) {
        const Rectangle r = bar_button(s_view, layout, i);
        const bool selected = game->selected_type == i;
        DrawRectangleRec(r, selected ? (Color){19, 47, 56, 255} : (Color){10, 25, 32, 255});
        DrawRectangleLinesEx(r, 1, selected ? colors[i] : (Color){39, 72, 78, 255});
        DrawRectangle((int)r.x, (int)r.y, selected ? (int)r.width : 18, 3, colors[i]);
        draw_text(names[i], (int)r.x + 6, row1 + 2, 11, RAYWHITE);
        draw_text(costs[i], (int)r.x + 6, row2, 11, C_GOLD);
    }
    const Rectangle pause = bar_button(s_view, layout, 3);
    DrawRectangleRec(pause, (Color){13, 36, 47, 255});
    DrawRectangleLinesEx(pause, 1, C_CYAN);
    DrawRectangle((int)pause.x + 12, (int)pause.y + 9, 7, (int)pause.height - 18,
                  (Color){181, 246, 245, 255});
    DrawRectangle((int)pause.x + 25, (int)pause.y + 9, 7, (int)pause.height - 18,
                  (Color){181, 246, 245, 255});
}

static void draw_purchase(const tower_game_t *game)
{
    if (game->phase != TOWER_PLAYING || game->selected_pad < 0) return;
    const tower_purchase_t quote = tower_game_purchase(game);
    const char *action = quote.action == TOWER_FEEDBACK_BUILT ? "BUILD"
        : quote.action == TOWER_FEEDBACK_UPGRADED ? "UPGRADE"
        : quote.action == TOWER_FEEDBACK_REPLACED ? "REPLACE" : "MAX LEVEL";
    Color color = game->credits >= quote.cost ? C_CYAN : C_RED;
    DrawRectangle(TOWER_CONFIRM_X,TOWER_CONFIRM_Y,TOWER_CONFIRM_W,TOWER_CONFIRM_H,C_BG);
    DrawRectangleLines(TOWER_CONFIRM_X,TOWER_CONFIRM_Y,TOWER_CONFIRM_W,TOWER_CONFIRM_H,color);
    draw_text(TextFormat("%s $%u",action,quote.cost),TOWER_CONFIRM_X+12,TOWER_CONFIRM_Y+8,16,color);
    if (quote.action == TOWER_FEEDBACK_REPLACED)
        draw_text(TextFormat("REFUND $%u INCLUDED",quote.refund),TOWER_CONFIRM_X+12,TOWER_CONFIRM_Y+32,10,C_GOLD);
    else draw_text("TAP TO CONFIRM",TOWER_CONFIRM_X+12,TOWER_CONFIRM_Y+32,10,RAYWHITE);
    DrawRectangle(TOWER_CANCEL_X,TOWER_CONFIRM_Y,TOWER_CANCEL_W,TOWER_CONFIRM_H,C_BG);
    DrawRectangleLines(TOWER_CANCEL_X,TOWER_CONFIRM_Y,TOWER_CANCEL_W,TOWER_CONFIRM_H,C_GOLD);
    draw_text("CANCEL",TOWER_CANCEL_X+8,TOWER_CONFIRM_Y+19,12,C_GOLD);
}

static void draw_messages(const tower_game_t *game)
{
    if (game->phase == TOWER_START)
        draw_overlay("CIRCUIT KEEP",
                     compact() ? "DEFEND THE CORE" : "BUILD TOWERS // DEFEND THE CORE",
                     "TOUCH TO START", C_CYAN);
    else if (game->phase == TOWER_PAUSED)
        draw_overlay("PAUSED", "TACTICAL GRID ON HOLD",
                     "TOUCH PAUSE TO RESUME", C_GOLD);
    else if (game->phase == TOWER_GAME_OVER)
        draw_overlay("CORE LOST",
                     TextFormat("WAVE %u // SCORE %05lu", game->wave,
                                (unsigned long)game->score),
                     "TOUCH TO RETRY", C_RED);
    else if (game->phase == TOWER_WON)
        draw_overlay("CORE SECURED", TextFormat("6 WAVES // SCORE %05lu",(unsigned long)game->score),
                     "TOUCH TO PLAY AGAIN", C_CYAN);
    else if (game->awaiting_wave && game->selected_pad < 0) {
        DrawRectangle(TOWER_WAVE_X,TOWER_WAVE_Y,TOWER_WAVE_W,TOWER_WAVE_H,C_BG);
        DrawRectangleLines(TOWER_WAVE_X,TOWER_WAVE_Y,TOWER_WAVE_W,TOWER_WAVE_H,C_GOLD);
        draw_centered(TextFormat("START %u/6",game->wave),TOWER_WAVE_Y+10,13,C_GOLD);
        draw_centered(TextFormat("%u ENEMIES",game->wave_total),TOWER_WAVE_Y+32,11,RAYWHITE);
    }
    else if (game->intermission && !game->awaiting_wave) {
        DrawRectangle(164, 374, 152, 18, (Color){4, 15, 22, 255});
        DrawRectangleLines(164, 374, 152, 18, (Color){77, 103, 100, 255});
        draw_centered(TextFormat("WAVE %u // T-%u", game->wave,
            (game->intermission + 29) / 30), 378, 11, C_GOLD);
    }
    draw_feedback(game);
    draw_purchase(game);
}

static void render_wide(const tower_view_t *view, tower_layout_t layout)
{
    const tower_game_t *game = view->game;
    const float scale = s_view.scale, zoom = scale * layout.k;
    raylib_lite_tilemap_draw_layer_scaled(view->map, 0,
        (raylib_lite_renderer_rect_t){0, 0, 480, 320},
        (raylib_lite_renderer_vec2_t){layout.mx * scale, layout.my * scale}, zoom);
    raylib_lite_design_begin(s_view);
    if (layout.mx > .5f) {
        DrawRectangle(0, (int)layout.my, (int)layout.mx + 1, (int)s_view.height, C_BG);
        DrawRectangle((int)(s_view.width - layout.mx), (int)layout.my, (int)layout.mx + 1,
                      (int)s_view.height, C_BG);
    }
    draw_bar(game, layout);
    raylib_lite_design_end(s_view);

    s_font = (raylib_lite_design_view_t){zoom, s_view.width / layout.k, s_view.height / layout.k};
    BeginMode2D((Camera2D){.offset = {layout.mx * scale, (layout.my - 69.0f * layout.k) * scale},
                           .zoom = zoom});
    BeginScissorMode(0, (int)(layout.my * scale), GetScreenWidth(), GetScreenHeight());
    draw_core(view);
    for (size_t i = 0; i < TOWER_PAD_COUNT; ++i) draw_pad(view, &game->towers[i]);
    draw_selected_range(game);
    for (size_t i = 0; i < TOWER_MAX_ENEMIES; ++i)
        if (game->enemies[i].active) draw_enemy(view->atlas, &game->enemies[i]);
    draw_projectiles(view);
    draw_effects(view);
    EndScissorMode();
    EndMode2D();
    s_font = s_view;

    raylib_lite_design_field_begin(s_view, FIELD, FIELD);
    draw_messages(game);
    raylib_lite_design_field_end(s_view, FIELD, FIELD);
}

void tower_view_map_pointer(const tower_game_t *game, float x, float y, float *field_x, float *field_y)
{
    const raylib_lite_design_view_t view = raylib_lite_design_view(FIELD);
    const tower_layout_t layout = layout_for(view);
    Vector2 point;
    if (!layout.wide) {
        point = raylib_lite_design_field_point(view, FIELD, FIELD, x, y);
    } else {
        Vector2 overlay = raylib_lite_design_field_point(view, FIELD, FIELD, x, y);
        if (game && game->phase==TOWER_PLAYING && game->awaiting_wave && game->selected_pad<0 &&
            overlay.x>=TOWER_WAVE_X && overlay.x<TOWER_WAVE_X+TOWER_WAVE_W &&
            overlay.y>=TOWER_WAVE_Y && overlay.y<TOWER_WAVE_Y+TOWER_WAVE_H) {
            *field_x=overlay.x; *field_y=overlay.y; return;
        }
        if (game && game->phase == TOWER_PLAYING && game->selected_pad >= 0 && overlay.y >= TOWER_CONFIRM_Y &&
            overlay.y < TOWER_CONFIRM_Y+TOWER_CONFIRM_H &&
            ((overlay.x >= TOWER_CONFIRM_X && overlay.x < TOWER_CONFIRM_X+TOWER_CONFIRM_W) ||
             (overlay.x >= TOWER_CANCEL_X && overlay.x < TOWER_CANCEL_X+TOWER_CANCEL_W))) {
            *field_x = overlay.x;
            *field_y = overlay.y;
            return;
        }
        point = raylib_lite_design_point(view, x, y);
        if (point.y < layout.my) {
            /* Bar taps become taps on the matching 480-layout control. */
            static const Vector2 targets[4] = {{85, 437}, {239, 437}, {393, 437}, {443, 32}};
            Vector2 target = {-1000, -1000};
            for (int i = 0; i < 4; ++i)
                if (CheckCollisionPointRec(point, bar_button(view, layout, i))) target = targets[i];
            point = target;
        } else {
            point = (Vector2){(point.x - layout.mx) / layout.k,
                              69.0f + (point.y - layout.my) / layout.k};
            /* A map point must not alias the differently positioned overlay. */
            if (game && game->phase == TOWER_PLAYING && game->selected_pad >= 0 &&
                point.y >= TOWER_CONFIRM_Y && point.y < TOWER_CONFIRM_Y+TOWER_CONFIRM_H &&
                point.x >= TOWER_CONFIRM_X && point.x < TOWER_CANCEL_X+TOWER_CANCEL_W)
                point = (Vector2){-1000,-1000};
        }
    }
    *field_x = point.x;
    *field_y = point.y;
}

void tower_view_render(const tower_view_t *view)
{
    if (!view || !view->game) return;
    const tower_game_t *game = view->game;
    BeginDrawing();
    if (!raylib_lite_raylib_frame_available()) {
        EndDrawing();
        return;
    }
    /* The opaque HUD (rows 0-68), the fully tiled terrain layer (69-388) and
     * the shop (392-479) cover the screen; only the gap above the shop shows
     * background. tests/test_tower_overdraw.py checks the map has no empty tile. */
    s_view = raylib_lite_design_view(FIELD);
    s_font = s_view;
    const tower_layout_t layout = layout_for(s_view);
    if (layout.wide) {
        render_wide(view, layout);
        EndDrawing();
        return;
    }
    const Vector2 field = raylib_lite_design_field_origin(s_view, FIELD, FIELD);
    if (s_view.width > FIELD + 1.0f || s_view.height > FIELD + 1.0f) {
        raylib_lite_design_begin(s_view);
        draw_wings(game->tick);
        raylib_lite_design_end(s_view);
    }
    raylib_lite_tilemap_draw_layer_scaled(view->map, 0,
        (raylib_lite_renderer_rect_t){0, 0, 480, 320},
        (raylib_lite_renderer_vec2_t){field.x * s_view.scale, (field.y + 69) * s_view.scale},
        s_view.scale);
    raylib_lite_design_field_begin(s_view, FIELD, FIELD);
    DrawRectangle(0, 389, 480, 3, C_GRASS);
    draw_core(view);
    for (size_t i = 0; i < TOWER_PAD_COUNT; ++i) draw_pad(view, &game->towers[i]);
    draw_selected_range(game);
    for (size_t i = 0; i < TOWER_MAX_ENEMIES; ++i)
        if (game->enemies[i].active) draw_enemy(view->atlas, &game->enemies[i]);
    draw_projectiles(view);
    draw_effects(view);
    draw_hud(game);
    draw_shop(game);
    draw_messages(game);
    raylib_lite_design_field_end(s_view, FIELD, FIELD);
    EndDrawing();
}
