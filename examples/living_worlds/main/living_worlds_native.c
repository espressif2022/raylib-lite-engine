// SPDX-License-Identifier: Apache-2.0
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "living_worlds_native.h"

#include "driver/jpeg_decode.h"
#include "esp_log.h"
#include "raylib_lite_example_board.h"
#include "mosaico_game_2d.h"
#include "mosaico_game_assets.h"
#include "raylib_lite_game_app.h"
#include "raylib_lite_native_hooks.h"
#include "living_worlds_session.h"

static const char *TAG = "living_worlds";

#define ATLAS_SYMBOLS(name) \
    extern const uint8_t _binary_##name##_atlas_start[]; \
    extern const uint8_t _binary_##name##_atlas_end[]
ATLAS_SYMBOLS(sunrise_cliff_front);
ATLAS_SYMBOLS(sunrise_cliff_side);
ATLAS_SYMBOLS(sunrise_cliff_rear);
ATLAS_SYMBOLS(aurora_ice_front);
ATLAS_SYMBOLS(aurora_ice_side);
ATLAS_SYMBOLS(aurora_ice_rear);
ATLAS_SYMBOLS(ocean_reef_left_front);
ATLAS_SYMBOLS(ocean_reef_left_side);
ATLAS_SYMBOLS(ocean_reef_left_rear);
ATLAS_SYMBOLS(ocean_reef_right_front);
ATLAS_SYMBOLS(ocean_reef_right_side);
ATLAS_SYMBOLS(ocean_reef_right_rear);
ATLAS_SYMBOLS(rainforest_falls);
#define SOUND_SYMBOLS(name) \
    extern const uint8_t _binary_##name##_sound_start[]; \
    extern const uint8_t _binary_##name##_sound_end[]
SOUND_SYMBOLS(aurora_wind_ice);
SOUND_SYMBOLS(ocean_ambience);
SOUND_SYMBOLS(sunrise_wind);
SOUND_SYMBOLS(rainforest_ambience);
extern const uint8_t _binary_ocean_jpg_start[], _binary_ocean_jpg_end[];
extern const uint8_t _binary_aurora_jpg_start[], _binary_aurora_jpg_end[];
extern const uint8_t _binary_sunrise_jpg_start[], _binary_sunrise_jpg_end[];
extern const uint8_t _binary_rainforest_jpg_start[], _binary_rainforest_jpg_end[];

typedef struct {
    raylib_lite_example_board_t *board;
    living_worlds_session_t session;
    jpeg_decoder_handle_t jpeg;
    void *background_pixels;
    esp_err_t asset_error;
} living_worlds_native_t;

static esp_err_t register_asset(const char *name, const uint8_t *start,
                                const uint8_t *end)
{
    return mosaico_game_asset_register_memory(name, start,
                                              (size_t)(end - start));
}

static esp_err_t register_assets(void)
{
#define REGISTER(name, symbol) \
    do { \
        esp_err_t err = register_asset(name, _binary_##symbol##_atlas_start, \
                                       _binary_##symbol##_atlas_end); \
        if (err != ESP_OK) return err; \
    } while (0)
    REGISTER("sunrise_cliff_front.atlas", sunrise_cliff_front);
    REGISTER("sunrise_cliff_side.atlas", sunrise_cliff_side);
    REGISTER("sunrise_cliff_rear.atlas", sunrise_cliff_rear);
    REGISTER("aurora_ice_front.atlas", aurora_ice_front);
    REGISTER("aurora_ice_side.atlas", aurora_ice_side);
    REGISTER("aurora_ice_rear.atlas", aurora_ice_rear);
    REGISTER("ocean_reef_left_front.atlas", ocean_reef_left_front);
    REGISTER("ocean_reef_left_side.atlas", ocean_reef_left_side);
    REGISTER("ocean_reef_left_rear.atlas", ocean_reef_left_rear);
    REGISTER("ocean_reef_right_front.atlas", ocean_reef_right_front);
    REGISTER("ocean_reef_right_side.atlas", ocean_reef_right_side);
    REGISTER("ocean_reef_right_rear.atlas", ocean_reef_right_rear);
    REGISTER("rainforest_falls.atlas", rainforest_falls);
#undef REGISTER
#define REGISTER_SOUND(name) \
    do { \
        esp_err_t err = register_asset(#name ".sound", \
            _binary_##name##_sound_start, _binary_##name##_sound_end); \
        if (err != ESP_OK) return err; \
    } while (0)
    REGISTER_SOUND(aurora_wind_ice);
    REGISTER_SOUND(ocean_ambience);
    REGISTER_SOUND(sunrise_wind);
    REGISTER_SOUND(rainforest_ambience);
#undef REGISTER_SOUND
    return ESP_OK;
}

static esp_err_t decode_background(jpeg_decoder_handle_t decoder,
                                   const char *name, const uint8_t *start,
                                   const uint8_t *end, MosaicoAtlas *out,
                                   void **out_pixels)
{
    jpeg_decode_picture_info_t info = {0};
    const size_t stream_size = (size_t)(end - start);
    esp_err_t err = jpeg_decoder_get_info(start, stream_size, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "parse %s JPEG: %s", name, esp_err_to_name(err));
        return err;
    }
    const size_t padded_width = (info.width + 15U) & ~15U;
    const size_t padded_height = (info.height + 15U) & ~15U;
    const size_t required = padded_width * padded_height * 2U;
    size_t allocated = 0;
    const jpeg_decode_memory_alloc_cfg_t memory = {
        .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
    };
    void *pixels = jpeg_alloc_decoder_mem(required, &memory, &allocated);
    if (!pixels || allocated < required) {
        free(pixels);
        return ESP_ERR_NO_MEM;
    }
    const jpeg_decode_cfg_t config = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t output_size = 0;
    err = jpeg_decoder_process(decoder, &config, start, stream_size, pixels,
                               allocated, &output_size);
    if (err != ESP_OK) {
        free(pixels);
        return err;
    }
    Texture2D texture = Mosaico2DRegisterRGB565(
        pixels, (int)info.width, (int)info.height);
    if (!texture.id) {
        free(pixels);
        return ESP_ERR_NO_MEM;
    }
    *out_pixels = pixels;
    *out = (MosaicoAtlas){.texture = texture};
    ESP_LOGI(TAG, "JPEG %s: %ux%u, %u bytes -> %u bytes RGB565", name,
             (unsigned)info.width, (unsigned)info.height,
             (unsigned)stream_size, (unsigned)output_size);
    return ESP_OK;
}

static void clear_background_aliases(living_worlds_atlases_t *atlases)
{
    atlases->aurora = (MosaicoAtlas){0};
    atlases->ocean = (MosaicoAtlas){0};
    atlases->sunrise = (MosaicoAtlas){0};
    atlases->rainforest = (MosaicoAtlas){0};
}

static esp_err_t load_background(living_worlds_native_t *state,
                                 living_worlds_atlases_t *atlases,
                                 uint8_t scene)
{
    const char *name;
    const uint8_t *start;
    const uint8_t *end;
    switch (scene) {
    case LIVING_SCENE_AURORA:
        name = "aurora";
        start = _binary_aurora_jpg_start;
        end = _binary_aurora_jpg_end;
        break;
    case LIVING_SCENE_SUNRISE:
        name = "sunrise";
        start = _binary_sunrise_jpg_start;
        end = _binary_sunrise_jpg_end;
        break;
    case LIVING_SCENE_RAINFOREST:
        name = "rainforest";
        start = _binary_rainforest_jpg_start;
        end = _binary_rainforest_jpg_end;
        break;
    default:
        scene = LIVING_SCENE_OCEAN;
        name = "ocean";
        start = _binary_ocean_jpg_start;
        end = _binary_ocean_jpg_end;
        break;
    }

    MosaicoAtlas next = {0};
    void *next_pixels = NULL;
    esp_err_t err = decode_background(state->jpeg, name, start, end,
                                      &next, &next_pixels);
    if (err != ESP_OK) return err;

    if (atlases->aurora.texture.id)
        Mosaico2DUnloadTexture(atlases->aurora.texture);
    free(state->background_pixels);
    clear_background_aliases(atlases);
    state->background_pixels = next_pixels;
    atlases->aurora = atlases->ocean =
        atlases->sunrise = atlases->rainforest = next;
    if (scene == LIVING_SCENE_OCEAN)
        (void)Mosaico2DCacheTextureLight(next.texture, 232);
    return ESP_OK;
}

static int session_load_background(void *context,
                                   living_worlds_atlases_t *atlases,
                                   uint8_t scene)
{
    living_worlds_native_t *state = context;
    state->asset_error = load_background(state, atlases, scene);
    return state->asset_error == ESP_OK ? 0 : -1;
}

static void session_release_background(void *context,
                                       living_worlds_atlases_t *atlases)
{
    living_worlds_native_t *state = context;
    if (atlases->aurora.texture.id)
        Mosaico2DUnloadTexture(atlases->aurora.texture);
    free(state->background_pixels);
    state->background_pixels = NULL;
    clear_background_aliases(atlases);
}

static const living_worlds_session_assets_t s_session_assets = {
    .load_background = session_load_background,
    .release_background = session_release_background,
};

static raylib_lite_result_t app_start(void *user)
{
    living_worlds_native_t *state = user;
    esp_err_t err = register_assets();
    if (err != ESP_OK) goto fail;
    const jpeg_decode_engine_cfg_t jpeg_config = {
        .intr_priority = 0,
        .timeout_ms = 250,
    };
    err = jpeg_new_decoder_engine(&jpeg_config, &state->jpeg);
    if (err != ESP_OK) goto fail;
    if (living_worlds_session_start(&state->session, &s_session_assets, state)) {
        err = state->asset_error == ESP_OK ? ESP_ERR_NOT_FOUND : state->asset_error;
        goto fail;
    }
    return RAYLIB_LITE_OK;

fail:
    ESP_LOGE(TAG, "startup failed: %s", esp_err_to_name(err));
    living_worlds_session_close(&state->session);
    if (state->jpeg) {
        jpeg_del_decoder_engine(state->jpeg);
        state->jpeg = NULL;
    }
    mosaico_game_assets_unmount();
    return err == ESP_ERR_NO_MEM ? RAYLIB_LITE_NO_MEMORY
                                 : RAYLIB_LITE_PLATFORM_ERROR;
}

static raylib_lite_result_t app_first_present(void *user)
{
    living_worlds_native_t *state = user;
    if (!state->board ||
            raylib_lite_example_board_start_input(state->board) != ESP_OK)
        return RAYLIB_LITE_PLATFORM_ERROR;
    raylib_lite_native_first_present();
    return RAYLIB_LITE_OK;
}

static void app_event(void *user, const raylib_lite_input_event_t *event)
{
    living_worlds_native_t *state = user;
    if (!event) return;
    if (event->type == RAYLIB_LITE_INPUT_POINTER ||
        event->type == RAYLIB_LITE_INPUT_TOUCH)
        living_worlds_session_pointer(&state->session, (float)event->x,
                                      (float)event->y, event->pressed);
    else if (event->type == RAYLIB_LITE_INPUT_BUTTON)
        living_worlds_session_action(&state->session, event->value,
                                     event->pressed);
}

static bool app_idle(void *user)
{
    return ((living_worlds_native_t *)user)->session.paused;
}

static void app_update(void *user)
{
    living_worlds_native_t *state = user;
    if (living_worlds_session_update(&state->session)) {
        for (unsigned i = 0; i < SCENE_AUDIO_COUNT; ++i) {
            if (!state->session.audio.tracks[i].frameCount)
                ESP_LOGW(TAG, "failed to load %s", SCENE_AUDIO_PATHS[i]);
        }
    }
}

static void app_render(void *user)
{
    living_worlds_session_render(&((living_worlds_native_t *)user)->session);
}

static void app_stop(void *user)
{
    living_worlds_native_t *state = user;
    living_worlds_session_close(&state->session);
    if (state->jpeg) {
        jpeg_del_decoder_engine(state->jpeg);
        state->jpeg = NULL;
    }
    mosaico_game_assets_unmount();
}

esp_err_t living_worlds_native_run(raylib_lite_example_board_t *board)
{
    if (!board) return ESP_ERR_INVALID_ARG;
    const raylib_lite_platform_t *services =
        raylib_lite_example_board_services(board);
    if (!services) return ESP_ERR_INVALID_STATE;

    living_worlds_native_t state = {.board = board};
    const raylib_lite_game_app_t app = {
        .tag = "living_worlds",
        .window_title = "Living Worlds",
        .logic_hz = 30,
        .target_fps = 30,
        /* Keep the shared timing logger dormant in release builds. */
        .stats_interval = UINT32_MAX,
        .platform = *services,
        .input = raylib_lite_example_board_input(board),
        .user = &state,
        .on_start = app_start,
        .on_first_present = app_first_present,
        .on_event = app_event,
        .idle = app_idle,
        .on_update = app_update,
        .on_render = app_render,
        .on_stats = NULL,
        .on_stop = app_stop,
    };
    switch (raylib_lite_game_app_run(&app)) {
    case RAYLIB_LITE_OK: return ESP_OK;
    case RAYLIB_LITE_INVALID_ARGUMENT: return ESP_ERR_INVALID_ARG;
    case RAYLIB_LITE_INVALID_STATE: return ESP_ERR_INVALID_STATE;
    case RAYLIB_LITE_NO_MEMORY: return ESP_ERR_NO_MEM;
    case RAYLIB_LITE_TIMEOUT: return ESP_ERR_TIMEOUT;
    case RAYLIB_LITE_NOT_SUPPORTED: return ESP_ERR_NOT_SUPPORTED;
    default: return ESP_FAIL;
    }
}
