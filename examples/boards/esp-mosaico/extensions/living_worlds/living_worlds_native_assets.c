// SPDX-License-Identifier: Apache-2.0
#include "living_worlds_native_assets.h"

#include <stdlib.h>
#include "driver/jpeg_decode.h"
#include "esp_log.h"
#include "raylib_lite_2d.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_example_board.h"

static const char *TAG = "living_worlds_assets";
static raylib_lite_result_t s_registration_result = RAYLIB_LITE_OK;

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

struct living_worlds_native_assets {
    jpeg_decoder_handle_t jpeg;
    void *background_pixels;
};

static void register_asset(const char *name, const uint8_t *start,
                           const uint8_t *end)
{
    if (s_registration_result != RAYLIB_LITE_OK) return;
    s_registration_result = raylib_lite_asset_register_memory(
        name, start, (size_t)(end - start));
}

void raylib_lite_register_native_assets(void)
{
    s_registration_result = RAYLIB_LITE_OK;
#define REGISTER_ATLAS(name) register_asset(#name ".atlas", \
    _binary_##name##_atlas_start, _binary_##name##_atlas_end)
    REGISTER_ATLAS(sunrise_cliff_front);
    REGISTER_ATLAS(sunrise_cliff_side);
    REGISTER_ATLAS(sunrise_cliff_rear);
    REGISTER_ATLAS(aurora_ice_front);
    REGISTER_ATLAS(aurora_ice_side);
    REGISTER_ATLAS(aurora_ice_rear);
    REGISTER_ATLAS(ocean_reef_left_front);
    REGISTER_ATLAS(ocean_reef_left_side);
    REGISTER_ATLAS(ocean_reef_left_rear);
    REGISTER_ATLAS(ocean_reef_right_front);
    REGISTER_ATLAS(ocean_reef_right_side);
    REGISTER_ATLAS(ocean_reef_right_rear);
    REGISTER_ATLAS(rainforest_falls);
#undef REGISTER_ATLAS
#define REGISTER_SOUND(name) register_asset(#name ".sound", \
    _binary_##name##_sound_start, _binary_##name##_sound_end)
    REGISTER_SOUND(aurora_wind_ice);
    REGISTER_SOUND(ocean_ambience);
    REGISTER_SOUND(sunrise_wind);
    REGISTER_SOUND(rainforest_ambience);
#undef REGISTER_SOUND
}

/* Preserve the established Living Worlds native Board tuning while using the
 * shared native launcher. This remains application-layer policy. */
void raylib_lite_example_game_board_config(raylib_lite_example_board_config_t *config)
{
    if (!config) return;
    *config = (raylib_lite_example_board_config_t) {
        .touch_points = 1,
        .enable_imu = false,
        .drawbuf_lines = 40,
        .drawbuf_count = 2,
    };
}

static raylib_lite_result_t from_esp_error(esp_err_t error)
{
    switch (error) {
    case ESP_OK: return RAYLIB_LITE_OK;
    case ESP_ERR_NO_MEM: return RAYLIB_LITE_NO_MEMORY;
    case ESP_ERR_INVALID_ARG: return RAYLIB_LITE_INVALID_ARGUMENT;
    case ESP_ERR_INVALID_STATE: return RAYLIB_LITE_INVALID_STATE;
    case ESP_ERR_TIMEOUT: return RAYLIB_LITE_TIMEOUT;
    default: return RAYLIB_LITE_PLATFORM_ERROR;
    }
}

raylib_lite_result_t living_worlds_native_assets_open(
    living_worlds_native_assets_t **out_assets)
{
    if (!out_assets) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_assets = NULL;
    if (s_registration_result != RAYLIB_LITE_OK) return s_registration_result;
    living_worlds_native_assets_t *assets = calloc(1, sizeof(*assets));
    if (!assets) return RAYLIB_LITE_NO_MEMORY;
    const jpeg_decode_engine_cfg_t config = {
        .intr_priority = 0,
        .timeout_ms = 250,
    };
    esp_err_t error = jpeg_new_decoder_engine(&config, &assets->jpeg);
    if (error != ESP_OK) {
        free(assets);
        return from_esp_error(error);
    }
    *out_assets = assets;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t decode_background(living_worlds_native_assets_t *assets,
    const char *name, const uint8_t *start, const uint8_t *end,
    raylib_lite_atlas_t *out, void **out_pixels)
{
    jpeg_decode_picture_info_t info = {0};
    const size_t stream_size = (size_t)(end - start);
    esp_err_t error = jpeg_decoder_get_info(start, stream_size, &info);
    if (error != ESP_OK) return from_esp_error(error);
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
        return RAYLIB_LITE_NO_MEMORY;
    }
    const jpeg_decode_cfg_t config = {
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    };
    uint32_t output_size = 0;
    error = jpeg_decoder_process(assets->jpeg, &config, start, stream_size,
                                 pixels, allocated, &output_size);
    if (error != ESP_OK) {
        free(pixels);
        return from_esp_error(error);
    }
    Texture2D texture = raylib_lite_2d_register_rgb565(
        pixels, (int)info.width, (int)info.height);
    if (!texture.id) {
        free(pixels);
        return RAYLIB_LITE_NO_MEMORY;
    }
    *out_pixels = pixels;
    *out = (raylib_lite_atlas_t){.texture = texture};
    ESP_LOGI(TAG, "JPEG %s: %ux%u, %u bytes -> %u bytes RGB565", name,
             (unsigned)info.width, (unsigned)info.height,
             (unsigned)stream_size, (unsigned)output_size);
    return RAYLIB_LITE_OK;
}

static void clear_background(living_worlds_native_assets_t *assets,
                             living_worlds_atlases_t *atlases)
{
    if (atlases->aurora.texture.id)
        raylib_lite_2d_unload_texture(atlases->aurora.texture);
    free(assets->background_pixels);
    assets->background_pixels = NULL;
    atlases->aurora = atlases->ocean = atlases->sunrise =
        atlases->rainforest = (raylib_lite_atlas_t){0};
}

static int load_background(void *context, living_worlds_atlases_t *atlases,
                           uint8_t scene)
{
    living_worlds_native_assets_t *assets = context;
    if (!assets) return -1;
    const char *name;
    const uint8_t *start, *end;
    switch (scene) {
    case LIVING_SCENE_AURORA:
        name = "aurora"; start = _binary_aurora_jpg_start; end = _binary_aurora_jpg_end; break;
    case LIVING_SCENE_SUNRISE:
        name = "sunrise"; start = _binary_sunrise_jpg_start; end = _binary_sunrise_jpg_end; break;
    case LIVING_SCENE_RAINFOREST:
        name = "rainforest"; start = _binary_rainforest_jpg_start; end = _binary_rainforest_jpg_end; break;
    default:
        scene = LIVING_SCENE_OCEAN;
        name = "ocean"; start = _binary_ocean_jpg_start; end = _binary_ocean_jpg_end; break;
    }
    raylib_lite_atlas_t next = {0};
    void *pixels = NULL;
    raylib_lite_result_t result = decode_background(
        assets, name, start, end, &next, &pixels);
    if (result != RAYLIB_LITE_OK) return -1;
    clear_background(assets, atlases);
    assets->background_pixels = pixels;
    atlases->aurora = atlases->ocean = atlases->sunrise = atlases->rainforest = next;
    if (scene == LIVING_SCENE_OCEAN)
        (void)raylib_lite_2d_cache_texture_light(next.texture, 232);
    return 0;
}

static void release_background(void *context, living_worlds_atlases_t *atlases)
{
    living_worlds_native_assets_t *assets = context;
    if (assets) clear_background(assets, atlases);
}

const living_worlds_session_assets_t *living_worlds_native_assets_callbacks(void)
{
    static const living_worlds_session_assets_t callbacks = {
        .load_background = load_background,
        .release_background = release_background,
    };
    return &callbacks;
}

void living_worlds_native_assets_close(living_worlds_native_assets_t *assets)
{
    if (!assets) return;
    free(assets->background_pixels);
    if (assets->jpeg) jpeg_del_decoder_engine(assets->jpeg);
    free(assets);
}
