// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_2d.h"

static raylib_lite_renderer_texture_t to_renderer_texture(Texture2D texture)
{
    return (raylib_lite_renderer_texture_t) {
        .id = texture.id,
        .width = texture.width,
        .height = texture.height,
    };
}

static Texture2D to_raylib_texture(raylib_lite_renderer_texture_t texture)
{
    if (!texture.id) return (Texture2D){0};
    return (Texture2D) {
        .id = texture.id,
        .width = texture.width,
        .height = texture.height,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
    };
}

static Rectangle to_raylib_rect(raylib_lite_renderer_rect_t rectangle)
{
    return (Rectangle) {
        rectangle.x, rectangle.y, rectangle.width, rectangle.height,
    };
}

static Vector2 to_raylib_vec2(raylib_lite_renderer_vec2_t vector)
{
    return (Vector2){vector.x, vector.y};
}

static raylib_lite_sprite_frame_t to_raylib_frame(
    const raylib_lite_renderer_sprite_frame_t *frame)
{
    return (raylib_lite_sprite_frame_t) {
        .id = frame->id,
        .source = to_raylib_rect(frame->source),
        .pivot = to_raylib_vec2(frame->pivot),
    };
}

Texture2D raylib_lite_2d_load_texture(const char *asset_path)
{
    return to_raylib_texture(raylib_lite_renderer_load_texture(asset_path));
}

Texture2D raylib_lite_2d_register_rgb565(const void *pixels, int width, int height)
{
    return to_raylib_texture(
        raylib_lite_renderer_register_rgb565(pixels, width, height));
}

bool raylib_lite_2d_cache_texture_light(Texture2D texture, unsigned light256)
{
    return raylib_lite_renderer_cache_texture_light(
        to_renderer_texture(texture), light256);
}

void raylib_lite_2d_unload_texture(Texture2D texture)
{
    raylib_lite_renderer_unload_texture(to_renderer_texture(texture));
}

raylib_lite_atlas_t raylib_lite_atlas_load(const char *asset_path)
{
    raylib_lite_renderer_atlas_t atlas = raylib_lite_renderer_load_atlas(asset_path);
    return (raylib_lite_atlas_t) {
        .texture = to_raylib_texture(atlas.texture),
        .descriptor = atlas.descriptor,
        .frame_count = atlas.frame_count,
    };
}

const raylib_lite_sprite_frame_t *raylib_lite_atlas_get_frame(raylib_lite_atlas_t atlas,
                                               raylib_lite_asset_id_t frame_id)
{
    static raylib_lite_sprite_frame_t result;
    const raylib_lite_renderer_sprite_frame_t *frame =
        raylib_lite_renderer_atlas_get_frame((raylib_lite_renderer_atlas_t) {
            .texture = to_renderer_texture(atlas.texture),
            .descriptor = atlas.descriptor,
            .frame_count = atlas.frame_count,
        }, frame_id);
    if (!frame) return NULL;
    result = to_raylib_frame(frame);
    return &result;
}

raylib_lite_result_t raylib_lite_atlas_get_frame_copy(raylib_lite_atlas_t atlas,
    raylib_lite_asset_id_t frame_id, raylib_lite_sprite_frame_t *out_frame)
{
    if (!out_frame) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_renderer_sprite_frame_t frame;
    raylib_lite_result_t error = raylib_lite_renderer_atlas_get_frame_copy(
        (raylib_lite_renderer_atlas_t) {
            .texture = to_renderer_texture(atlas.texture),
            .descriptor = atlas.descriptor,
            .frame_count = atlas.frame_count,
        }, frame_id, &frame);
    if (error == RAYLIB_LITE_OK) *out_frame = to_raylib_frame(&frame);
    return error;
}

raylib_lite_result_t raylib_lite_wall_atlas_get_frame(raylib_lite_wall_atlas_t atlas,
    raylib_lite_asset_id_t frame_id, raylib_lite_sprite_frame_t *out_frame)
{
    if (!out_frame) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_renderer_sprite_frame_t frame;
    raylib_lite_result_t error = raylib_lite_renderer_wall_atlas_get_frame(
        atlas, frame_id, &frame);
    if (error == RAYLIB_LITE_OK) *out_frame = to_raylib_frame(&frame);
    return error;
}

void raylib_lite_atlas_unload(raylib_lite_atlas_t atlas)
{
    raylib_lite_renderer_unload_atlas((raylib_lite_renderer_atlas_t) {
        .texture = to_renderer_texture(atlas.texture),
        .descriptor = atlas.descriptor,
        .frame_count = atlas.frame_count,
    });
}
