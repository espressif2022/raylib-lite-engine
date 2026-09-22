// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "esp_log.h"
#include "mosaico_game_assets.h"
#include "mosaico_game.h"
#include "tomb_app.h"
#include "tomb_game.h"
#include "tomb_view.h"
#include <math.h>

static tomb_game_t s_game;
static MosaicoWallAtlas s_textures;
static MosaicoAtlas s_controls;
static int32_t s_joystick_track = -1, s_look_track = -1, s_jump_track = -1;
static float s_move_forward, s_move_strafe;
static int32_t s_stick_x, s_stick_y;
static int32_t s_look_x, s_look_y;
static float s_display_fps;

#define TAG "tomb_explorer"
#define JOYSTICK_RADIUS 53

extern const uint8_t _binary_textures_wall_start[], _binary_textures_wall_end[];
extern const uint8_t _binary_controls_atlas_start[], _binary_controls_atlas_end[];

static esp_err_t embed(const char *name, const uint8_t *start, const uint8_t *end)
{
    esp_err_t err = mosaico_game_asset_register_memory(name, start, (size_t)(end - start));
    if (err != ESP_OK)
        ESP_LOGE(TAG, "embed %s failed: %s", name, esp_err_to_name(err));
    return err;
}

static esp_err_t before_display(void)
{
    esp_err_t err = embed("textures.wall", _binary_textures_wall_start, _binary_textures_wall_end);
    if (err != ESP_OK) return err;
    err = embed("controls.atlas", _binary_controls_atlas_start, _binary_controls_atlas_end);
    if (err != ESP_OK) return err;
    s_textures = LoadMosaicoWallAtlas("textures.wall");
    s_controls = LoadMosaicoAtlas("controls.atlas");
    if (!s_textures.descriptor || !s_controls.texture.id) {
        ESP_LOGE(TAG, "asset load failed textures=%p controls=%u",
                 s_textures.descriptor, s_controls.texture.id);
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

static void clear_tracks(void)
{
    s_joystick_track = s_look_track = s_jump_track = -1;
    s_move_forward = s_move_strafe = 0.0f;
    s_stick_x = TOMB_MOVE_X;
    s_stick_y = TOMB_MOVE_Y;
    tomb_set_jump(&s_game, false);
}

static esp_err_t on_start(void)
{
    tomb_reset(&s_game);
    clear_tracks();
    return ESP_OK;
}

static void update_joystick(int32_t x, int32_t y)
{
    float dx = (float)(x - s_stick_x) / (float)JOYSTICK_RADIUS;
    float dy = (float)(y - s_stick_y) / (float)JOYSTICK_RADIUS;
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 1.0f) { dx /= length; dy /= length; }
    if (length < .08f) dx = dy = 0.0f;
    s_move_strafe = dx;
    s_move_forward = -dy;
}

static void begin_joystick(int32_t track, int32_t x, int32_t y)
{
    s_joystick_track = track;
    s_stick_x = x;
    s_stick_y = y;
    update_joystick(x, y);
}

static void on_event(const mosaico_device_event_t *event)
{
#if CONFIG_TOMB_EXPLORER_BENCHMARK_MODE
    (void)event; /* The fixed replay must not be perturbed by live input. */
    return;
#else
    if (event->type != MOSAICO_DEVICE_EVENT_POINTER &&
        event->type != MOSAICO_DEVICE_EVENT_TOUCH) return;
    const int32_t track = event->value;
    if (!event->pressed) {
        if (track == s_joystick_track) {
            s_joystick_track = -1;
            s_move_forward = s_move_strafe = 0.0f;
        }
        if (track == s_look_track) s_look_track = -1;
        if (track == s_jump_track) {
            s_jump_track = -1;
            tomb_set_jump(&s_game, false);
        }
        return;
    }
    if (track == s_joystick_track) {
        update_joystick(event->x, event->y);
    } else if (track == s_look_track) {
        tomb_set_look(&s_game, (float)(event->x - s_look_x) * 0.008f,
                      (float)(event->y - s_look_y) * -0.005f);
        s_look_x = event->x;
        s_look_y = event->y;
    } else if (track == s_jump_track) {
        return;
    } else if (tomb_in_jump_zone(event->x, event->y) && s_jump_track < 0) {
        s_jump_track = track;
        tomb_set_jump(&s_game, true);
    } else if (tomb_in_move_capture(event->x, event->y) && s_joystick_track < 0) {
        begin_joystick(track, event->x, event->y);
    } else if (event->x >= TOMB_LOOK_MIN_X && s_look_track < 0) {
        s_look_track = track;
        s_look_x = event->x;
        s_look_y = event->y;
    }
#endif
}

static void on_update(void)
{
#if CONFIG_TOMB_EXPLORER_BENCHMARK_MODE
    /* Fixed 300-tick replay, 10 s at 30 Hz. Walking the loop while sweeping
     * the camera keeps the run passing through several rooms, so the raster
     * sample covers varied geometry instead of one static view. */
    const uint32_t phase = s_game.tick % 300U;
    s_move_forward = phase < 225U ? 1.0f : 0.25f;
    s_move_strafe = phase < 150U ? 0.0f : (phase < 225U ? 0.7f : -0.7f);
    tomb_set_look(&s_game, phase < 150U ? 0.012f : -0.008f, 0.0f);
    tomb_set_jump(&s_game, s_game.tick % 90U == 11U);
#endif
    tomb_set_stick(&s_game, s_move_strafe, -s_move_forward);
    tomb_update(&s_game);
}

static void on_render(void)
{
    mosaico_game_stats_t stats = {0};
    MosaicoGameGetStats(&stats);
    if (stats.display_fps > 0.5f) s_display_fps = stats.display_fps;
    tomb_hud_input_t input = {
        .stick_active = s_joystick_track >= 0,
        .jump_active = s_jump_track >= 0,
        .stick_x = s_stick_x,
        .stick_y = s_stick_y,
        .display_fps = s_display_fps,
    };
    tomb_view_render(&s_game, s_textures, s_controls, &input);
}

static void on_stats(void)
{
    mosaico_game_2d_raster_stats_t raster={0};
    mosaico_game_2d_get_raster_stats(&raster);
    ESP_LOGI(TAG, "pos=%.2f,%.2f,%.2f room=%u cam_room=%u state_hash=%08lx",
             s_game.x, s_game.y, s_game.z, (unsigned)s_game.room,
             (unsigned)s_game.camera_room, (unsigned long)tomb_state_hash(&s_game));
    ESP_LOGI(TAG, "view setup=%uus emit=%uus raster=%uus hud=%uus tris=%lu/%lu quads=%lu/%lu",
             (unsigned)raster.sky_us,(unsigned)raster.floor_us,
             (unsigned)raster.wall_us,(unsigned)raster.hud_us,
             (unsigned long)raster.triangle_calls,
             (unsigned long)raster.triangle_pixels,
             (unsigned long)raster.quad_calls,
             (unsigned long)raster.quad_pixels);
}

static const mosaico_game_app_config_t s_config = {
    .tag = "tomb_explorer",
    .window_title = "Tomb Explorer",
    .canvas_bind = GSP_TOMB_EXPLORER_BIND_GAME_CANVAS,
    .touch_points = 2,
    .enable_imu = false,
    .logic_hz = 30,
    .target_fps = 50,
    .gsp_bundle = gsp_bundle_config,
    .before_display = before_display,
    .on_start = on_start,
    .on_event = on_event,
    .on_update = on_update,
    .on_render = on_render,
    .on_stats = on_stats,
};

const mosaico_game_app_config_t *tomb_app_config(void)
{
    return &s_config;
}
