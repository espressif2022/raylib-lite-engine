// SPDX-License-Identifier: Apache-2.0
#include "sky_hop_view.h"

#include <math.h>
#include "assets_ids.h"
#include "raylib_lite_design_view.h"
#include "raylib_lite_raylib.h"

#define FLOOR_Y 390

/* Design space: 480 units tall; wider panels show more course. */
static raylib_lite_design_view_t s_view = {1.0f, 480.0f, 480.0f};
#define SCREEN_W ((int)s_view.width)
#define SCREEN_H ((int)s_view.height)

static int font(int size) { return raylib_lite_design_font(s_view, size); }
static void draw_text(const char *text, int x, int y, int size, Color color)
{
    DrawText(text, x, y, font(size), color);
}
static int measure_text(const char *text, int size) { return MeasureText(text, font(size)); }

typedef enum { WEATHER_BIRDS, WEATHER_RAIN, WEATHER_SNOW, WEATHER_EMBERS } weather_t;

typedef struct {
    Color sky_top, sky_bottom;
    Color mountain, mountain_cap;
    Color hill, hill_dark;
    Color cloud, cloud_shade;
    Color soil, soil_dark;
    Color accent;
    Color sun;
    int sun_x, sun_y, sun_r;
    weather_t weather;
} theme_t;

static const theme_t THEMES[PLATFORM_LEVEL_COUNT] = {
    { /* Sunny meadow */
        {64, 150, 228, 255}, {182, 230, 248, 255},
        {124, 164, 206, 255}, {226, 236, 246, 255},
        {98, 182, 102, 255}, {44, 104, 70, 255},
        {250, 253, 255, 255}, {206, 228, 242, 255},
        {124, 82, 50, 255}, {90, 58, 36, 255},
        {129, 224, 171, 255}, {255, 236, 140, 255}, 388, 92, 30, WEATHER_BIRDS,
    },
    { /* Storm ridge */
        {26, 32, 66, 255}, {96, 96, 140, 255},
        {50, 54, 92, 255}, {84, 90, 130, 255},
        {42, 76, 86, 255}, {18, 30, 44, 255},
        {92, 98, 134, 255}, {62, 66, 100, 255},
        {86, 66, 58, 255}, {58, 44, 40, 255},
        {150, 196, 255, 255}, {0, 0, 0, 0}, 0, 0, 0, WEATHER_RAIN,
    },
    { /* Frost peaks */
        {112, 182, 234, 255}, {228, 244, 252, 255},
        {150, 180, 214, 255}, {250, 252, 255, 255},
        {206, 228, 244, 255}, {120, 152, 186, 255},
        {255, 255, 255, 255}, {214, 230, 244, 255},
        {112, 104, 118, 255}, {80, 74, 90, 255},
        {120, 220, 255, 255}, {255, 250, 226, 255}, 110, 110, 24, WEATHER_SNOW,
    },
    { /* Ember crag */
        {54, 14, 36, 255}, {236, 104, 56, 255},
        {72, 22, 40, 255}, {255, 120, 60, 255},
        {98, 34, 40, 255}, {34, 10, 20, 255},
        {120, 60, 70, 255}, {86, 38, 52, 255},
        {84, 40, 36, 255}, {52, 24, 26, 255},
        {255, 170, 80, 255}, {255, 132, 72, 255}, 240, 214, 56, WEATHER_EMBERS,
    },
};

static uint32_t hash_u32(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

static int wrap(int value, int period)
{
    int result = value % period;
    return result < 0 ? result + period : result;
}

static const theme_t *theme_of(const platform_game_t *game)
{
    return &THEMES[game->level < PLATFORM_LEVEL_COUNT ? game->level : 0];
}

static void centered(const char *text, int y, int size, Color color)
{
    draw_text(text, (SCREEN_W - measure_text(text, size)) / 2, y, size, color);
}

static void shadowed(const char *text, int x, int y, int size, Color color)
{
    draw_text(text, x + 2, y + 2, size, (Color){0, 0, 0, 120});
    draw_text(text, x, y, size, color);
}

static void centered_shadowed(const char *text, int y, int size, Color color)
{
    shadowed(text, (SCREEN_W - measure_text(text, size)) / 2, y, size, color);
}

static void draw_sprite(raylib_lite_atlas_t atlas, raylib_lite_asset_id_t id, float x, float y,
                        float width, float height, bool flip)
{
    const raylib_lite_sprite_frame_t *frame = raylib_lite_atlas_get_frame(atlas, id);
    if (!frame) return;
    Rectangle source = frame->source;
    if (flip) source.width = -source.width;
    DrawTexturePro(atlas.texture, source, (Rectangle){x, y, width, height},
                   (Vector2){0, 0}, 0, WHITE);
}

static Color unpack(uint32_t packed)
{
    return (Color){(uint8_t)packed, (uint8_t)(packed >> 8),
                   (uint8_t)(packed >> 16), (uint8_t)(packed >> 24)};
}

static uint32_t pack(Color color)
{
    return (uint32_t)color.r | ((uint32_t)color.g << 8) |
           ((uint32_t)color.b << 16) | ((uint32_t)color.a << 24);
}

/* --- effects ------------------------------------------------------------ */

void sky_hop_fx_reset(sky_hop_fx_t *fx)
{
    *fx = (sky_hop_fx_t){0};
    raylib_lite_particle_pool_init(&fx->pool, fx->particles, SKY_HOP_PARTICLE_COUNT);
    fx->overlay_y = -240.0f;
}

static void burst(sky_hop_fx_t *fx, float x, float y, Color color, unsigned count,
                  float speed, float lift, float gravity, uint16_t life)
{
    for (unsigned i = 0; i < count; ++i) {
        uint32_t noise = hash_u32(fx->frame * 31U + i * 7919U);
        float angle = (float)i / (float)count * 6.2831853f + (float)(noise & 63U) * .01f;
        float magnitude = speed * (.6f + (float)((noise >> 8) & 63U) / 120.0f);
        (void)raylib_lite_particle_spawn(&fx->pool, (raylib_lite_particle_t){
            x, y, cosf(angle) * magnitude, sinf(angle) * magnitude * .6f - lift,
            gravity, pack(color), (uint16_t)(life + (noise >> 16) % 8U), true});
    }
}

static void confetti(sky_hop_fx_t *fx, float x, float y, unsigned count)
{
    static const Color colors[] = {
        {255, 220, 80, 255}, {255, 110, 140, 255}, {120, 220, 255, 255}, {129, 224, 171, 255},
    };
    for (unsigned i = 0; i < count; ++i)
        burst(fx, x, y, colors[i % 4], 1, 3.4f, 3.0f, .18f, 34);
}

static void popup(sky_hop_fx_t *fx, float x, float y, uint16_t value,
                  sky_hop_popup_kind_t kind, uint8_t chain)
{
    sky_hop_popup_t *slot = &fx->popups[0];
    for (size_t i = 0; i < SKY_HOP_POPUP_COUNT; ++i) {
        if (!fx->popups[i].ttl) { slot = &fx->popups[i]; break; }
        if (fx->popups[i].ttl < slot->ttl) slot = &fx->popups[i];
    }
    *slot = (sky_hop_popup_t){x, y, value, 34, (uint8_t)kind, chain};
}

void sky_hop_fx_events(sky_hop_fx_t *fx, const platform_game_t *game, uint32_t events)
{
    const float feet_x = game->player_x + 14, feet_y = game->player_y + 36;
    if (events & PLATFORM_EVENT_JUMP)
        burst(fx, feet_x, feet_y, (Color){240, 240, 230, 220}, 5, 1.6f, .4f, .04f, 12);
    if (events & PLATFORM_EVENT_LAND) {
        fx->squash_ticks = 6;
        burst(fx, feet_x, feet_y, (Color){236, 228, 214, 220}, 6, 2.0f, .3f, .03f, 12);
        if (game->land_air_ticks > 26U) fx->shake_ticks = 4;
    }
    if (events & PLATFORM_EVENT_SPRING)
        burst(fx, feet_x, feet_y, (Color){255, 232, 92, 255}, 8, 2.6f, 1.0f, .08f, 16);
    if (events & PLATFORM_EVENT_COIN) {
        burst(fx, game->last_gain_x, game->last_gain_y, (Color){255, 226, 90, 255},
              7, 2.2f, 1.2f, .10f, 14);
        popup(fx, game->last_gain_x, game->last_gain_y - 14, game->last_gain,
              SKY_HOP_POPUP_COIN, game->coin_chain);
    }
    if (events & PLATFORM_EVENT_STOMP) {
        burst(fx, game->last_gain_x, game->last_gain_y + 10, (Color){205, 125, 255, 255},
              10, 3.0f, 1.0f, .14f, 18);
        popup(fx, game->last_gain_x, game->last_gain_y - 10, game->last_gain,
              SKY_HOP_POPUP_STOMP, game->stomp_chain);
        fx->shake_ticks = 5;
    }
    if (events & PLATFORM_EVENT_CHECKPOINT) {
        float x = platform_game_checkpoint_x(game);
        confetti(fx, x + 20, 312, 14);
        popup(fx, x + 20, 288, game->last_gain, SKY_HOP_POPUP_BONUS, 0);
    }
    if (events & (PLATFORM_EVENT_HURT | PLATFORM_EVENT_GAME_OVER)) {
        float y = game->hit_y > 420 ? 420 : game->hit_y;
        burst(fx, game->hit_x, y, (Color){255, 95, 80, 255}, 14, 3.2f, 1.6f, .16f, 20);
        fx->shake_ticks = 10;
        fx->flash_ticks = 8;
    }
    if (events & (PLATFORM_EVENT_LEVEL_CLEAR | PLATFORM_EVENT_WON))
        confetti(fx, feet_x, feet_y - 30, 24);
    if (events & PLATFORM_EVENT_START) fx->banner_ticks = 84;
}

void sky_hop_fx_update(sky_hop_fx_t *fx, const platform_game_t *game)
{
    ++fx->frame;
    raylib_lite_particle_pool_update(&fx->pool);
    for (size_t i = 0; i < SKY_HOP_POPUP_COUNT; ++i) {
        if (!fx->popups[i].ttl) continue;
        --fx->popups[i].ttl;
        fx->popups[i].y -= fx->popups[i].ttl > 20 ? 1.4f : .5f;
    }
    if (fx->shake_ticks) --fx->shake_ticks;
    if (fx->squash_ticks) --fx->squash_ticks;
    if (fx->flash_ticks) --fx->flash_ticks;
    if (fx->banner_ticks && game->phase == PLATFORM_PLAYING) --fx->banner_ticks;

    bool want = game->phase != PLATFORM_PLAYING;
    if (want && !fx->overlay_shown) {
        fx->overlay_shown = true;
        fx->overlay_y = -240.0f;
        raylib_lite_tween_start(&fx->overlay_tween, -240.0f, 104.0f, 14, RAYLIB_LITE_EASE_OUT);
    } else if (!want && fx->overlay_shown) {
        fx->overlay_shown = false;
        fx->overlay_y = -240.0f;
        fx->overlay_tween.active = false;
    }
    if (fx->overlay_shown && fx->overlay_tween.active)
        fx->overlay_y = raylib_lite_tween_tick(&fx->overlay_tween);
}

/* --- backdrop ----------------------------------------------------------- */

static void draw_sun(const theme_t *theme)
{
    if (!theme->sun_r) return;
    DrawCircle(theme->sun_x, theme->sun_y, (float)theme->sun_r + 22, Fade(theme->sun, .14f));
    DrawCircle(theme->sun_x, theme->sun_y, (float)theme->sun_r + 10, Fade(theme->sun, .26f));
    DrawCircle(theme->sun_x, theme->sun_y, (float)theme->sun_r, theme->sun);
    if (theme->weather == WEATHER_EMBERS) {
        for (int band = 0; band < 4; ++band)
            DrawRectangle(theme->sun_x - theme->sun_r, theme->sun_y + 8 + band * 11,
                          theme->sun_r * 2, 2 + band, Fade(theme->sky_bottom, .85f));
    }
}

static void draw_mountains(const theme_t *theme, int camera)
{
    const int period = 150, scroll = camera / 6;
    for (int k = scroll / period - 1; k <= scroll / period + SCREEN_W / period + 1; ++k) {
        uint32_t noise = hash_u32((uint32_t)k * 2654435761U);
        float x = (float)(k * period - scroll);
        float peak = 300.0f - 70.0f - (float)(noise % 70U);
        float half = 92.0f + (float)((noise >> 8) % 34U);
        float cx = x + period * .5f;
        DrawTriangle((Vector2){cx, peak}, (Vector2){cx - half, 302},
                     (Vector2){cx + half, 302}, theme->mountain);
        float cap = (302.0f - peak) * .26f;
        float spread = half * cap / (302.0f - peak);
        DrawTriangle((Vector2){cx, peak}, (Vector2){cx - spread, peak + cap},
                     (Vector2){cx + spread, peak + cap}, theme->mountain_cap);
    }
}

static void draw_clouds(const theme_t *theme, int camera, uint32_t frame)
{
    int drift = (int)(frame / 6U);
    const int clouds = SCREEN_W > 560 ? 5 : 4;
    for (int i = 0; i < clouds; ++i) {
        int x = wrap(i * 170 - camera / 4 - drift * (1 + i % 2), SCREEN_W + 200) - 120;
        int y = 108 + (i % 3) * 34;
        DrawRectangleRounded((Rectangle){(float)x, (float)y + 2, 112, 20}, 1.0f, 6,
                             theme->cloud_shade);
        DrawCircle(x + 32, y, 17, theme->cloud);
        DrawCircle(x + 62, y - 8, 22, theme->cloud);
        DrawCircle(x + 88, y + 2, 14, theme->cloud);
        DrawRectangleRounded((Rectangle){(float)x + 6, (float)y - 2, 100, 18}, 1.0f, 6,
                             theme->cloud);
    }
}

static void draw_hills(const theme_t *theme, int camera)
{
    DrawRectangleGradientV(0, 336, SCREEN_W, SCREEN_H - 336, theme->hill, theme->hill_dark);
    const int period = 130, scroll = camera / 3;
    for (int k = scroll / period - 1; k <= scroll / period + SCREEN_W / period + 2; ++k) {
        uint32_t noise = hash_u32((uint32_t)k * 40503U + 17U);
        int x = k * period - scroll + (int)(noise % 40U);
        int radius = 58 + (int)((noise >> 8) % 26U);
        DrawCircle(x, 352 + (int)((noise >> 16) % 18U), (float)radius, theme->hill);
    }
}

static void draw_weather(const theme_t *theme, int camera, uint32_t frame)
{
    switch (theme->weather) {
    case WEATHER_BIRDS:
        for (int i = 0; i < 3; ++i) {
            int x = wrap(i * 190 + (int)frame * (2 + i % 2) - camera / 5, SCREEN_W + 140) - 70;
            int y = 70 + i * 26 + (int)(sinf((float)frame * .05f + (float)i) * 6.0f);
            int wing = ((frame / 6U + (uint32_t)i) & 1U) ? -5 : 2;
            Color ink = {46, 70, 104, 255};
            DrawLine(x - 7, y + wing, x, y, ink);
            DrawLine(x, y, x + 7, y + wing, ink);
        }
        break;
    case WEATHER_RAIN:
        for (int i = 0; i < 26 * SCREEN_W / 480; ++i) {
            int x = wrap(i * 53 - (int)frame * 6 - camera / 2, SCREEN_W + 60) - 30;
            int y = wrap(i * 97 + (int)frame * 18, 440) + 20;
            DrawLine(x, y, x - 5, y + 13, (Color){150, 176, 220, 255});
        }
        break;
    case WEATHER_SNOW:
        for (int i = 0; i < 28 * SCREEN_W / 480; ++i) {
            int sway = (int)(sinf((float)frame * .05f + (float)i) * 10.0f);
            int x = wrap(i * 61 + sway - camera / 3, SCREEN_W + 20) - 10;
            int y = wrap(i * 43 + (int)frame * (1 + i % 3), 420) + 44;
            int size = 2 + i % 2;
            DrawRectangle(x, y, size, size, RAYWHITE);
        }
        break;
    case WEATHER_EMBERS:
        for (int i = 0; i < 18 * SCREEN_W / 480; ++i) {
            int sway = (int)(sinf((float)frame * .07f + (float)i * 1.7f) * 8.0f);
            int x = wrap(i * 67 + sway - camera / 2, SCREEN_W + 20) - 10;
            int y = 440 - wrap(i * 71 + (int)frame * (1 + i % 2) * 2, 400);
            DrawRectangle(x, y, 2, 2, (i & 1) ? (Color){255, 196, 92, 255}
                                              : (Color){255, 110, 60, 255});
        }
        break;
    }
}

static void draw_backdrop(const platform_game_t *game, const sky_hop_fx_t *fx, int camera)
{
    const theme_t *theme = theme_of(game);
    Color top = theme->sky_top, bottom = theme->sky_bottom;
    if (theme->weather == WEATHER_RAIN && fx->frame % 170U < 4U) {
        top = ColorBrightness(top, .55f);
        bottom = ColorBrightness(bottom, .35f);
    }
    DrawRectangleGradientV(0, 0, SCREEN_W, 336, top, bottom);
    draw_sun(theme);
    draw_mountains(theme, camera);
    draw_clouds(theme, camera, fx->frame);
    draw_hills(theme, camera);
}

/* --- world -------------------------------------------------------------- */

static void draw_blocks(const platform_game_t *game, const theme_t *theme,
                        raylib_lite_atlas_t atlas, int camera)
{
    size_t block_count = 0;
    const platform_block_t *blocks = platform_game_blocks(game, &block_count);
    for (size_t i = 0; i < block_count; ++i) {
        int x = (int)blocks[i].x, width = (int)blocks[i].width;
        if (x + width < camera || x >= camera + SCREEN_W) continue;
        int first = x, last = x + width;
        if (blocks[i].y >= FLOOR_Y) {
            int y = (int)blocks[i].y + 48;
            DrawRectangle(x, y, width, 42, theme->soil);
            DrawRectangle(x, y + 12, width, 3, theme->soil_dark);
            int left = camera > x ? camera : x;
            for (int pebble = left - wrap(left, 37); pebble < x + width &&
                 pebble < camera + SCREEN_W; pebble += 37) {
                if (pebble < x) continue;
                uint32_t noise = hash_u32((uint32_t)pebble);
                DrawRectangle(pebble + (int)(noise % 20U), y + 20 + (int)((noise >> 8) % 16U),
                              5 + (int)((noise >> 16) % 4U), 3, theme->soil_dark);
            }
            first = camera - 48 > x ? camera - wrap(camera - x, 48) : x;
            if (camera + SCREEN_W < last) last = camera + SCREEN_W;
        }
        for (int tile_x = first; tile_x < last; tile_x += 48)
            draw_sprite(atlas, RAYLIB_LITE_ASSET_ID_TERRAIN_NATIVE, (float)tile_x,
                        blocks[i].y - 2, 50, 50, false);
    }
}

static void draw_springs(const platform_game_t *game)
{
    size_t count = 0;
    const platform_spring_t *springs = platform_game_springs(game, &count);
    for (size_t i = 0; i < count; ++i) {
        int x = (int)springs[i].x, y = (int)springs[i].y, w = (int)springs[i].width;
        int lift = i < PLATFORM_SPRING_COUNT ? game->spring_ticks[i] : 0;
        int plate = y - 8 - lift;
        DrawRectangle(x + 4, y - 4, w - 8, 4, (Color){80, 60, 72, 255});
        for (int coil = 0; coil < 3; ++coil) {
            int cy = plate + 8 + coil * (y - 4 - plate - 8) / 3;
            DrawLine(x + 10, cy, x + w - 10, cy + 3, (Color){255, 232, 92, 255});
        }
        DrawRectangleRounded((Rectangle){(float)x, (float)plate, (float)w, 8}, .5f, 4,
                             (Color){238, 73, 105, 255});
        DrawRectangle(x + 4, plate + 1, w - 8, 2, (Color){255, 150, 170, 255});
    }
}

static void draw_checkpoint(const platform_game_t *game, uint32_t frame)
{
    float x = platform_game_checkpoint_x(game);
    bool on = game->checkpoint_active;
    Color pole = on ? (Color){48, 220, 144, 255} : (Color){98, 116, 136, 255};
    Color flag = on ? (Color){72, 244, 169, 255} : (Color){150, 164, 176, 255};
    if (on) DrawCircle((int)x + 2, 304, 10, Fade(flag, .3f));
    DrawRectangle((int)x, 302, 5, 88, pole);
    float wave = sinf((float)frame * .2f) * 3.0f;
    DrawTriangle((Vector2){x + 5, 304}, (Vector2){x + 5, 334},
                 (Vector2){x + 42, 319 + wave}, flag);
    DrawCircle((int)x + 2, 300, 4, (Color){255, 220, 80, 255});
}

static void draw_coins(const platform_game_t *game, raylib_lite_atlas_t atlas, uint32_t frame)
{
    static const raylib_lite_asset_id_t coin_frames[] = {
        RAYLIB_LITE_ASSET_ID_COIN_25, RAYLIB_LITE_ASSET_ID_COIN_27,
        RAYLIB_LITE_ASSET_ID_COIN_29};
    for (size_t i = 0; i < game->coin_count; ++i) {
        if (game->coins[i].collected) continue;
        float bob = sinf((float)frame * .12f + (float)i) * 3.0f;
        int x = (int)game->coins[i].x, y = (int)(game->coins[i].y + bob);
        unsigned pulse_index = (unsigned)((frame / 5U + i) % 3U);
        float size = 25.0f + (float)pulse_index * 2.0f;
        DrawCircle(x, y, 14, (Color){255, 226, 90, 46});
        draw_sprite(atlas, coin_frames[pulse_index], (float)x - size / 2, (float)y - size / 2,
                    size, size, false);
        if ((frame / 4U + i * 5U) % 24U == 0U)
            draw_sprite(atlas, RAYLIB_LITE_ASSET_ID_SPARKLE, (float)x + 2, (float)y - 16,
                        12, 12, false);
    }
}

static void draw_enemies(const platform_game_t *game, raylib_lite_atlas_t atlas, uint32_t frame)
{
    for (size_t i = 0; i < game->enemy_count; ++i) {
        const platform_enemy_t *enemy = &game->enemies[i];
        if (!enemy->active) continue;
        float squash = ((frame / 4U + i) & 1U) ? 3.0f : 0.0f;
        DrawEllipse((int)enemy->x + 14, (int)enemy->y + 29, 16, 4, (Color){0, 0, 0, 60});
        draw_sprite(atlas, RAYLIB_LITE_ASSET_ID_ENEMY_BEETLE_NATIVE,
                    enemy->x - 7 - squash / 2, enemy->y - 12 + squash, 44 + squash,
                    44 - squash, enemy->speed < 0);
    }
}

static void draw_hero(const platform_game_t *game, const sky_hop_fx_t *fx,
                      raylib_lite_atlas_t atlas)
{
    if (game->invulnerable_ticks && ((game->invulnerable_ticks / 3U) & 1U)) return;
    raylib_lite_asset_id_t hero = !game->grounded ? RAYLIB_LITE_ASSET_ID_HERO_JUMP_NATIVE
        : (game->move_left || game->move_right)
        ? ((game->tick / MOSAICO_ANIMATION_HERO_RUN_FRAME_TICKS) & 1U
           ? RAYLIB_LITE_ASSET_ID_HERO_RUN_NATIVE : RAYLIB_LITE_ASSET_ID_HERO_IDLE_NATIVE)
        : RAYLIB_LITE_ASSET_ID_HERO_IDLE_NATIVE;
    float width = 58.0f, height = 64.0f;
    if (!game->grounded) {
        float stretch = fabsf(game->velocity_y) / 90.0f;
        if (stretch > .12f) stretch = .12f;
        width *= 1.0f - stretch;
        height *= 1.0f + stretch;
    } else if (fx->squash_ticks) {
        float squash = (float)fx->squash_ticks / 40.0f;
        width *= 1.0f + squash;
        height *= 1.0f - squash;
    }
    float center = game->player_x + 14, feet = game->player_y + 42;
    if (game->grounded)
        DrawEllipse((int)center, (int)game->player_y + 37, 14, 4, (Color){0, 0, 0, 70});
    draw_sprite(atlas, hero, center - width / 2, feet - height, width, height,
                game->facing_left);
}

static void draw_finish(const platform_game_t *game, raylib_lite_atlas_t atlas,
                        int camera, uint32_t frame)
{
    const float finish_x = platform_game_finish_x(game);
    if (finish_x - (float)camera > (float)SCREEN_W + 20) return;
    float glow = 18.0f + sinf((float)frame * .15f) * 4.0f;
    DrawCircle((int)finish_x + 22, 330, glow + 10, (Color){255, 236, 140, 40});
    draw_sprite(atlas, RAYLIB_LITE_ASSET_ID_FINISH_FLAG_NATIVE, finish_x - 12, 292, 70, 98,
                false);
    if ((frame / 6U) % 3U == 0U)
        draw_sprite(atlas, RAYLIB_LITE_ASSET_ID_SPARKLE, finish_x + 30, 296, 14, 14, false);
}

static void draw_particles(const sky_hop_fx_t *fx)
{
    for (size_t i = 0; i < SKY_HOP_PARTICLE_COUNT; ++i) {
        const raylib_lite_particle_t *particle = &fx->particles[i];
        if (!particle->active) continue;
        int size = 1 + (particle->life > 8 ? 2 : 1);
        DrawRectangle((int)particle->x - size / 2, (int)particle->y - size / 2,
                      size + 1, size + 1, unpack(particle->color));
    }
}

static void draw_popups(const sky_hop_fx_t *fx, int camera, int shake_x)
{
    for (size_t i = 0; i < SKY_HOP_POPUP_COUNT; ++i) {
        const sky_hop_popup_t *item = &fx->popups[i];
        if (!item->ttl) continue;
        Color color = item->kind == SKY_HOP_POPUP_STOMP ? (Color){222, 160, 255, 255}
            : item->kind == SKY_HOP_POPUP_BONUS ? (Color){129, 244, 181, 255}
            : (Color){255, 226, 90, 255};
        if (item->ttl < 8) color = Fade(color, (float)item->ttl / 8.0f);
        const char *text = item->kind == SKY_HOP_POPUP_STOMP && item->chain > 1
            ? TextFormat("+%u x%u", item->value, item->chain)
            : item->kind == SKY_HOP_POPUP_BONUS ? TextFormat("CHECKPOINT +%u", item->value)
            : TextFormat("+%u", item->value);
        int size = item->kind == SKY_HOP_POPUP_COIN ? 14 : 16;
        int x = (int)item->x - camera + shake_x - measure_text(text, size) / 2;
        shadowed(text, x, (int)item->y, size, color);
    }
}

/* --- interface ---------------------------------------------------------- */

static void draw_heart(int x, int y, Color color)
{
    DrawCircle(x - 4, y, 5, color);
    DrawCircle(x + 4, y, 5, color);
    DrawTriangle((Vector2){(float)x - 9, (float)y + 1}, (Vector2){(float)x, (float)y + 11},
                 (Vector2){(float)x + 9, (float)y + 1}, color);
}

static void draw_hud(const sky_hop_view_t *view, const theme_t *theme)
{
    const platform_game_t *game = view->game;
    DrawRectangle(0, 0, SCREEN_W, 44, (Color){14, 26, 46, 200});
    DrawRectangle(0, 44, SCREEN_W, 1, Fade(theme->accent, .6f));
    for (int i = 0; i < 3; ++i)
        draw_heart(20 + i * 22, 15, i < game->lives ? (Color){255, 86, 104, 255}
                                                     : (Color){60, 70, 92, 255});
    draw_sprite(view->atlas, RAYLIB_LITE_ASSET_ID_COIN_25, 10, 25, 15, 15, false);
    draw_text(TextFormat("%u/%u", game->coins_collected, game->coin_count), 29, 28, 12,
             (Color){255, 226, 90, 255});

    const char *score = TextFormat("%06lu", (unsigned long)game->score);
    shadowed(score, (SCREEN_W - measure_text(score, 22)) / 2, 5, 22, RAYWHITE);
    const char *level = TextFormat("%u/%u %s", game->level + 1, PLATFORM_LEVEL_COUNT,
                                   platform_game_level_name(game));
    centered(level, raylib_lite_design_view_native(s_view) ? 30 : 25, 10, theme->accent);

    draw_text("BEST", SCREEN_W - 150, 8, 10, (Color){170, 186, 210, 255});
    draw_text(TextFormat("%06lu", (unsigned long)view->best_score), SCREEN_W - 150, 21, 14,
             (Color){255, 220, 80, 255});

    if (game->coin_chain > 1U || game->stomp_chain > 1U) {
        bool stomp = game->stomp_chain > 1U;
        const char *combo = TextFormat("%s x%u", stomp ? "STOMP" : "COMBO",
                                       stomp ? game->stomp_chain : game->coin_chain);
        int width = measure_text(combo, 12) + 16;
        DrawRectangleRounded((Rectangle){(float)(SCREEN_W - width) / 2, 50, (float)width, 18},
                             .5f, 4, stomp ? (Color){120, 60, 170, 220}
                                           : (Color){170, 120, 20, 220});
        centered(combo, 53, 12, RAYWHITE);
    }

    const int pause_x = SCREEN_W - 44;
    DrawRectangleRounded((Rectangle){(float)pause_x, 5, 36, 34}, .3f, 4, (Color){52, 77, 104, 255});
    DrawRectangle(pause_x + 11, 13, 5, 18, RAYWHITE);
    DrawRectangle(pause_x + 21, 13, 5, 18, RAYWHITE);

    /* Course progress: start, checkpoint and finish along the top edge. */
    float finish = platform_game_finish_x(game);
    float progress = game->player_x / finish;
    if (progress > 1.0f) progress = 1.0f;
    float checkpoint = platform_game_checkpoint_x(game) / finish;
    DrawRectangle(0, 45, SCREEN_W, 3, (Color){20, 30, 50, 160});
    DrawRectangle(0, 45, (int)(progress * SCREEN_W), 3, theme->accent);
    DrawRectangle((int)(checkpoint * SCREEN_W) - 1, 44, 3, 5,
                  game->checkpoint_active ? (Color){72, 244, 169, 255}
                                          : (Color){180, 190, 200, 255});
}

static void draw_button(Rectangle bounds, Color accent, bool pressed, int icon)
{
    float sink = pressed ? 3.0f : 0.0f;
    Rectangle face = {bounds.x, bounds.y + sink, bounds.width, bounds.height - sink};
    if (!pressed)
        DrawRectangleRounded((Rectangle){bounds.x, bounds.y + 4, bounds.width, bounds.height},
                             .3f, 6, (Color){0, 0, 0, 70});
    DrawRectangleRounded(face, .3f, 6, pressed ? Fade(accent, .78f) : Fade(accent, .42f));
    DrawRectangleRoundedLines(face, .3f, 6, Fade(RAYWHITE, pressed ? .8f : .45f));
    float cx = face.x + face.width / 2, cy = face.y + face.height / 2;
    Color ink = Fade(RAYWHITE, pressed ? 1.0f : .9f);
    if (icon < 0) {
        DrawTriangle((Vector2){cx - 16, cy}, (Vector2){cx + 10, cy + 15},
                     (Vector2){cx + 10, cy - 15}, ink);
    } else if (icon > 0) {
        DrawTriangle((Vector2){cx + 16, cy}, (Vector2){cx - 10, cy - 15},
                     (Vector2){cx - 10, cy + 15}, ink);
    } else {
        DrawTriangle((Vector2){cx, cy - 18}, (Vector2){cx - 18, cy + 2},
                     (Vector2){cx + 18, cy + 2}, ink);
        draw_text("JUMP", (int)cx - measure_text("JUMP", 12) / 2, (int)cy + 8, 12, ink);
    }
}

static void draw_controls(const platform_game_t *game)
{
    Color steel = {38, 62, 96, 255};
    DrawRectangle(0,400,SCREEN_W,80,(Color){14,26,46,255});
    for(unsigned action=0;action<3;++action){
        platform_block_t r=platform_game_control_rect(game,(platform_action_t)action);
        bool held=action==0?game->move_left:action==1?game->move_right:game->jump_held;
        draw_button((Rectangle){r.x,r.y,r.width,r.height},
                    action==2?(Color){236,104,63,255}:steel,held,
                    action==0?-1:action==1?1:0);
    }
}

static void draw_banner(const platform_game_t *game, const sky_hop_fx_t *fx,
                        const theme_t *theme)
{
    if (!fx->banner_ticks || game->phase != PLATFORM_PLAYING) return;
    int age = 84 - fx->banner_ticks;
    float slide = age < 10 ? (float)(10 - age) / 10.0f
                : fx->banner_ticks < 10 ? (float)(10 - fx->banner_ticks) / 10.0f : 0.0f;
    int x = (int)(-slide * 300.0f) + (SCREEN_W - 300) / 2;
    DrawRectangleRounded((Rectangle){(float)x, 150, 300, 58}, .3f, 6, (Color){14, 26, 46, 210});
    DrawRectangle(x + 12, 202, 276, 2, theme->accent);
    const char *title = TextFormat("LEVEL %u", game->level + 1);
    shadowed(title, x + 150 - measure_text(title, 22) / 2, 158, 22, RAYWHITE);
    const char *name = platform_game_level_name(game);
    draw_text(name, x + 150 - measure_text(name, 14) / 2, 184, 14, theme->accent);
}

static void draw_hint(const platform_game_t *game)
{
    if(game->phase!=PLATFORM_PLAYING||game->level!=0||game->player_x>600||game->tick<84U)return;
    const char *hint=game->player_x<300?"HOLD A DIRECTION TO RUN":"HOLD JUMP TO CLEAR THE STEP";
    int width = measure_text(hint, 13) + 24;
    DrawRectangleRounded((Rectangle){(float)(SCREEN_W - width) / 2, 76, (float)width, 25}, .4f, 4,
                         (Color){14, 26, 46, 210});
    centered(hint, 82, 13, RAYWHITE);
}

static void draw_overlay(const sky_hop_view_t *view, const theme_t *theme)
{
    const platform_game_t *game = view->game;
    const sky_hop_fx_t *fx = view->fx;
    int top = (int)fx->overlay_y;
    const float ox = (float)(SCREEN_W - 480) / 2;
    Rectangle panel = {ox + 40, (float)top, 400, 236};
    DrawRectangleRounded((Rectangle){panel.x, panel.y + 6, panel.width, panel.height}, .12f, 8,
                         (Color){0, 0, 0, 90});
    DrawRectangleRounded(panel, .12f, 8, (Color){18, 34, 60, 245});
    DrawRectangleRoundedLines(panel, .12f, 8, theme->accent);

    const char *title = game->phase == PLATFORM_WON ? "ALL CLEAR!"
        : game->phase == PLATFORM_LEVEL_CLEAR ? "LEVEL CLEAR!"
        : game->phase == PLATFORM_GAME_OVER ? "GAME OVER"
        : game->phase == PLATFORM_PAUSED ? "PAUSED" : "SKY HOP";
    Color gold = {255, 220, 80, 255};
    centered_shadowed(title, top + 22, 40, game->phase == PLATFORM_GAME_OVER
                      ? (Color){255, 110, 110, 255} : gold);

    if (game->phase == PLATFORM_START) {
        float bob = sinf((float)fx->frame * .15f) * 6.0f;
        /* Scaled text fills the panel; the mascots only fit at 1:1. */
        if (raylib_lite_design_view_native(s_view)) {
            draw_sprite(view->atlas, RAYLIB_LITE_ASSET_ID_HERO_JUMP_NATIVE, ox + 90,
                        (float)top + 80 + bob, 58, 64, false);
            draw_sprite(view->atlas, RAYLIB_LITE_ASSET_ID_COIN_29, ox + 340,
                        (float)top + 92 - bob, 29, 29, false);
        }
        int y = top + 84;
        centered("FOUR LANDS. COINS. STOMPS.", y, 14, RAYWHITE);
        y += font(14) + 8;
        centered("CHAIN COINS AND STOMPS", y, 12, (Color){180, 200, 224, 255});
        y += font(12) + 4;
        centered("FOR BIGGER BONUSES", y, 12, (Color){180, 200, 224, 255});
        if (view->best_score)
            centered(TextFormat("BEST %06lu", (unsigned long)view->best_score),
                     y + font(12) + 16, 14, gold);
    } else if (game->phase == PLATFORM_PAUSED) {
        centered(TextFormat("%s  %u/%u COINS", platform_game_level_name(game),
                            game->coins_collected, game->coin_count), top + 96, 16, RAYWHITE);
        centered(TextFormat("SCORE %06lu", (unsigned long)game->score), top + 124, 16, gold);
    } else {
        centered(TextFormat("SCORE %06lu", (unsigned long)game->score), top + 82, 20, RAYWHITE);
        if (game->phase == PLATFORM_LEVEL_CLEAR || game->phase == PLATFORM_WON) {
            centered(TextFormat("COINS %u/%u   STOMPS %u", game->coins_collected,
                                game->coin_count, game->stomps), top + 112, 15,
                     (Color){180, 200, 224, 255});
            for (int i = 0; i < 3; ++i) {
                bool earned = game->coins_collected * 3U >= game->coin_count * (unsigned)(i + 1);
                float cx = ox + 200.0f + (float)i * 40.0f, cy = (float)top + 152;
                DrawPoly((Vector2){cx, cy}, 5, 15, -90, earned ? gold : (Color){60, 74, 100, 255});
            }
        } else {
            centered(TextFormat("BEST %06lu", (unsigned long)view->best_score), top + 114, 16, gold);
        }
    }
    const char *prompt = game->phase == PLATFORM_PAUSED ? "TOUCH TO RESUME"
        : game->phase == PLATFORM_LEVEL_CLEAR ? "TOUCH FOR NEXT LEVEL"
        : game->phase == PLATFORM_WON || game->phase == PLATFORM_GAME_OVER
        ? "TOUCH TO PLAY AGAIN" : "TOUCH TO START";
    float blink = .75f + .25f * sinf((float)fx->frame * .18f);
    centered(prompt, top + 190, 21, Fade((Color){129, 224, 171, 255}, blink));
}

bool sky_hop_view_render(const sky_hop_view_t *view)
{
    if (!view || !view->game || !view->fx) return false;
    const platform_game_t *game = view->game;
    const sky_hop_fx_t *fx = view->fx;
    const theme_t *theme = theme_of(game);
    const int camera = (int)game->camera_x;
    int shake_x = 0, shake_y = 0;
    if (fx->shake_ticks) {
        uint32_t noise = hash_u32(fx->frame);
        int amount = fx->shake_ticks > 4 ? 4 : fx->shake_ticks;
        shake_x = (int)(noise % (uint32_t)(amount * 2 + 1)) - amount;
        shake_y = (int)((noise >> 8) % (uint32_t)(amount * 2 + 1)) - amount;
    }
    Camera2D world_camera = {.offset = {(float)shake_x, (float)shake_y},
                             .target = {game->camera_x, 0}, .rotation = 0, .zoom = 1};
    BeginDrawing();
    if (!raylib_lite_raylib_frame_available()) {
        EndDrawing();
        return false;
    }
    s_view = raylib_lite_design_view(480.0f);
    raylib_lite_design_begin(s_view);
    draw_backdrop(game, fx, camera);
    raylib_lite_design_end(s_view);

    BeginMode2D(raylib_lite_design_camera(s_view, world_camera));
    draw_blocks(game, theme, view->atlas, camera);
    draw_springs(game);
    draw_checkpoint(game, fx->frame);
    draw_coins(game, view->atlas, fx->frame);
    draw_enemies(game, view->atlas, fx->frame);
    draw_finish(game, view->atlas, camera, fx->frame);
    draw_hero(game, fx, view->atlas);
    draw_particles(fx);
    EndMode2D();

    raylib_lite_design_begin(s_view);
    draw_weather(theme, camera, fx->frame);
    draw_popups(fx, camera, shake_x);
    if (fx->flash_ticks) {
        Color hurt = Fade((Color){255, 40, 60, 255}, (float)fx->flash_ticks / 20.0f);
        DrawRectangle(0, 0, SCREEN_W, 10, hurt);
        DrawRectangle(0, SCREEN_H - 10, SCREEN_W, 10, hurt);
        DrawRectangle(0, 10, 10, SCREEN_H - 20, hurt);
        DrawRectangle(SCREEN_W - 10, 10, 10, SCREEN_H - 20, hurt);
    }
    draw_hud(view, theme);
    draw_banner(game, fx, theme);
    draw_hint(game);
    draw_controls(game);
    if (game->phase != PLATFORM_PLAYING) draw_overlay(view, theme);
    raylib_lite_design_end(s_view);
    EndDrawing();
    return true;
}
