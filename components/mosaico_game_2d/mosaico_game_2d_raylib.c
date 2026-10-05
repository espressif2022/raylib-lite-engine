// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_2d.h"

static mosaico_renderer_texture_t to_renderer_texture(Texture2D texture)
{
    return (mosaico_renderer_texture_t) {
        .id = texture.id,
        .width = texture.width,
        .height = texture.height,
    };
}

static Texture2D to_raylib_texture(mosaico_renderer_texture_t texture)
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

static Rectangle to_raylib_rect(mosaico_renderer_rect_t rectangle)
{
    return (Rectangle) {
        rectangle.x, rectangle.y, rectangle.width, rectangle.height,
    };
}

static Vector2 to_raylib_vec2(mosaico_renderer_vec2_t vector)
{
    return (Vector2){vector.x, vector.y};
}

static MosaicoSpriteFrame to_raylib_frame(
    const mosaico_renderer_sprite_frame_t *frame)
{
    return (MosaicoSpriteFrame) {
        .id = frame->id,
        .source = to_raylib_rect(frame->source),
        .pivot = to_raylib_vec2(frame->pivot),
    };
}

Texture2D Mosaico2DLoadTexture(const char *asset_path)
{
    return to_raylib_texture(mosaico_renderer_load_texture(asset_path));
}

Texture2D Mosaico2DRegisterRGB565(const void *pixels, int width, int height)
{
    return to_raylib_texture(
        mosaico_renderer_register_rgb565(pixels, width, height));
}

bool Mosaico2DCacheTextureLight(Texture2D texture, unsigned light256)
{
    return mosaico_renderer_cache_texture_light(
        to_renderer_texture(texture), light256);
}

void Mosaico2DUnloadTexture(Texture2D texture)
{
    mosaico_renderer_unload_texture(to_renderer_texture(texture));
}

MosaicoAtlas LoadMosaicoAtlas(const char *asset_path)
{
    mosaico_renderer_atlas_t atlas = mosaico_renderer_load_atlas(asset_path);
    return (MosaicoAtlas) {
        .texture = to_raylib_texture(atlas.texture),
        .descriptor = atlas.descriptor,
        .frame_count = atlas.frame_count,
    };
}

const MosaicoSpriteFrame *MosaicoAtlasGetFrame(MosaicoAtlas atlas,
                                               mosaico_asset_id_t frame_id)
{
    static MosaicoSpriteFrame result;
    const mosaico_renderer_sprite_frame_t *frame =
        mosaico_renderer_atlas_get_frame((mosaico_renderer_atlas_t) {
            .texture = to_renderer_texture(atlas.texture),
            .descriptor = atlas.descriptor,
            .frame_count = atlas.frame_count,
        }, frame_id);
    if (!frame) return NULL;
    result = to_raylib_frame(frame);
    return &result;
}

esp_err_t mosaico_game_2d_atlas_get_frame(MosaicoAtlas atlas,
    mosaico_asset_id_t frame_id, MosaicoSpriteFrame *out_frame)
{
    if (!out_frame) return ESP_ERR_INVALID_ARG;
    mosaico_renderer_sprite_frame_t frame;
    esp_err_t error = mosaico_renderer_atlas_get_frame_copy(
        (mosaico_renderer_atlas_t) {
            .texture = to_renderer_texture(atlas.texture),
            .descriptor = atlas.descriptor,
            .frame_count = atlas.frame_count,
        }, frame_id, &frame);
    if (error == ESP_OK) *out_frame = to_raylib_frame(&frame);
    return error;
}

esp_err_t mosaico_game_2d_wall_atlas_get_frame(MosaicoWallAtlas atlas,
    mosaico_asset_id_t frame_id, MosaicoSpriteFrame *out_frame)
{
    if (!out_frame) return ESP_ERR_INVALID_ARG;
    mosaico_renderer_sprite_frame_t frame;
    esp_err_t error = mosaico_renderer_wall_atlas_get_frame(
        atlas, frame_id, &frame);
    if (error == ESP_OK) *out_frame = to_raylib_frame(&frame);
    return error;
}

void UnloadMosaicoAtlas(MosaicoAtlas atlas)
{
    mosaico_renderer_unload_atlas((mosaico_renderer_atlas_t) {
        .texture = to_renderer_texture(atlas.texture),
        .descriptor = atlas.descriptor,
        .frame_count = atlas.frame_count,
    });
}
