// SPDX-License-Identifier: Apache-2.0
#include "living_worlds_native_assets.h"

#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#if SOC_JPEG_DECODE_SUPPORTED
#include "driver/jpeg_decode.h"
#else
/* Targets without a JPEG engine use the TJpgDec decoder in ROM. */
#include "esp32s3/rom/tjpgd.h"
#endif
#include "raylib_lite_2d.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_example_board.h"

static const char *TAG = "living_worlds_assets";

struct living_worlds_native_assets {
#if SOC_JPEG_DECODE_SUPPORTED
    jpeg_decoder_handle_t jpeg;
#endif
    void *background_pixels;
};

/* Game-side native requirements consumed through the abstract example-Board
 * contract. No concrete Board API is referenced here. */
void raylib_lite_example_game_board_config(raylib_lite_example_board_config_t *config)
{
    if (!config) return;
    *config = (raylib_lite_example_board_config_t) {
        .touch_points = 1,
        .enable_imu = false,
        .drawbuf_lines = 34,
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
    living_worlds_native_assets_t *assets = calloc(1, sizeof(*assets));
    if (!assets) return RAYLIB_LITE_NO_MEMORY;
#if SOC_JPEG_DECODE_SUPPORTED
    const jpeg_decode_engine_cfg_t config = {
        .intr_priority = 0,
        .timeout_ms = 250,
    };
    esp_err_t error = jpeg_new_decoder_engine(&config, &assets->jpeg);
    if (error != ESP_OK) {
        free(assets);
        return from_esp_error(error);
    }
#endif
    *out_assets = assets;
    return RAYLIB_LITE_OK;
}

#if SOC_JPEG_DECODE_SUPPORTED
static raylib_lite_result_t decode_jpeg(living_worlds_native_assets_t *assets,
    const raylib_lite_asset_view_t *view, void **out_pixels,
    unsigned *out_width, unsigned *out_height)
{
    jpeg_decode_picture_info_t info = {0};
    esp_err_t error = jpeg_decoder_get_info(view->data, view->size, &info);
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
    error = jpeg_decoder_process(assets->jpeg, &config, view->data, view->size,
                                 pixels, allocated, &output_size);
    if (error != ESP_OK) {
        free(pixels);
        return from_esp_error(error);
    }
    *out_pixels = pixels;
    *out_width = info.width;
    *out_height = info.height;
    return RAYLIB_LITE_OK;
}
#else
typedef struct {
    const uint8_t *data;
    size_t size, offset;
    uint16_t *pixels;
    unsigned width;
} tjpgd_session_t;

static UINT tjpgd_input(JDEC *decoder, BYTE *buffer, UINT count)
{
    tjpgd_session_t *session = decoder->device;
    size_t left = session->size - session->offset;
    if (count > left) count = (UINT)left;
    if (buffer) memcpy(buffer, session->data + session->offset, count);
    session->offset += count;
    return count;
}

static UINT tjpgd_output(JDEC *decoder, void *bitmap, JRECT *rect)
{
    tjpgd_session_t *session = decoder->device;
    const uint8_t *rgb = bitmap;
    for (unsigned y = rect->top; y <= rect->bottom; ++y) {
        uint16_t *dst = session->pixels + (size_t)y * session->width + rect->left;
        for (unsigned x = rect->left; x <= rect->right; ++x, rgb += 3)
            *dst++ = (uint16_t)(((rgb[0] & 0xf8U) << 8) | ((rgb[1] & 0xfcU) << 3) | (rgb[2] >> 3));
    }
    return 1;
}

static raylib_lite_result_t decode_jpeg(living_worlds_native_assets_t *assets,
    const raylib_lite_asset_view_t *view, void **out_pixels,
    unsigned *out_width, unsigned *out_height)
{
    (void)assets;
    enum { TJPGD_WORK_BYTES = 4096 };
    void *work = heap_caps_malloc(TJPGD_WORK_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!work) return RAYLIB_LITE_NO_MEMORY;
    tjpgd_session_t session = {.data = view->data, .size = view->size};
    JDEC decoder;
    JRESULT status = jd_prepare(&decoder, tjpgd_input, work, TJPGD_WORK_BYTES, &session);
    if (status != JDR_OK) {
        heap_caps_free(work);
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    session.width = decoder.width;
    session.pixels = heap_caps_malloc((size_t)decoder.width * decoder.height * 2U,
                                      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!session.pixels) {
        heap_caps_free(work);
        return RAYLIB_LITE_NO_MEMORY;
    }
    status = jd_decomp(&decoder, tjpgd_output, 0);
    heap_caps_free(work);
    if (status != JDR_OK) {
        heap_caps_free(session.pixels);
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    *out_pixels = session.pixels;
    *out_width = decoder.width;
    *out_height = decoder.height;
    return RAYLIB_LITE_OK;
}
#endif

static raylib_lite_result_t decode_background(living_worlds_native_assets_t *assets,
    const char *name, raylib_lite_atlas_t *out, void **out_pixels)
{
    raylib_lite_asset_view_t source = {0};
    raylib_lite_result_t result = raylib_lite_asset_open(name, &source);
    if (result != RAYLIB_LITE_OK) return result;
    const size_t stream_size = source.size;
    void *pixels = NULL;
    unsigned width = 0, height = 0;
    result = decode_jpeg(assets, &source, &pixels, &width, &height);
    raylib_lite_asset_release(&source);
    if (result != RAYLIB_LITE_OK) return result;
    Texture2D texture = raylib_lite_2d_register_rgb565(
        pixels, (int)width, (int)height);
    if (!texture.id) {
        free(pixels);
        return RAYLIB_LITE_NO_MEMORY;
    }
    *out_pixels = pixels;
    *out = (raylib_lite_atlas_t){.texture = texture};
    ESP_LOGI(TAG, "JPEG %s: %ux%u, %u bytes -> %u bytes RGB565", name,
             width, height, (unsigned)stream_size, width * height * 2U);
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
    switch (scene) {
    case LIVING_SCENE_AURORA: name = "aurora.jpg"; break;
    case LIVING_SCENE_SUNRISE: name = "sunrise.jpg"; break;
    case LIVING_SCENE_RAINFOREST: name = "rainforest.jpg"; break;
    default: scene = LIVING_SCENE_OCEAN; name = "ocean.jpg"; break;
    }
    raylib_lite_atlas_t next = {0};
    void *pixels = NULL;
    raylib_lite_result_t result = decode_background(assets, name, &next, &pixels);
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
#if SOC_JPEG_DECODE_SUPPORTED
    if (assets->jpeg) jpeg_del_decoder_engine(assets->jpeg);
#endif
    free(assets);
}
