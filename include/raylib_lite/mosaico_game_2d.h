// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Legacy Raylib-shaped Mosaico2D facade. The raster core contract lives in
 * mosaico_renderer.h and does not depend on Raylib types. */
#include "mosaico_renderer.h"
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    mosaico_asset_id_t id;
    Rectangle source;
    Vector2 pivot;
} MosaicoSpriteFrame;

typedef struct {
    Texture2D texture;
    const void *descriptor;
    uint16_t frame_count;
} MosaicoAtlas;

MosaicoAtlas LoadMosaicoAtlas(const char *asset_path);
const MosaicoSpriteFrame *MosaicoAtlasGetFrame(MosaicoAtlas atlas,
                                               mosaico_asset_id_t frame_id);
esp_err_t mosaico_game_2d_atlas_get_frame(MosaicoAtlas atlas,
    mosaico_asset_id_t frame_id, MosaicoSpriteFrame *out_frame);
esp_err_t mosaico_game_2d_wall_atlas_get_frame(MosaicoWallAtlas atlas,
    mosaico_asset_id_t frame_id, MosaicoSpriteFrame *out_frame);
void UnloadMosaicoAtlas(MosaicoAtlas atlas);

Texture2D Mosaico2DLoadTexture(const char *asset_path);
Texture2D Mosaico2DRegisterRGB565(const void *pixels, int width, int height);
bool Mosaico2DCacheTextureLight(Texture2D texture, unsigned light256);
void Mosaico2DUnloadTexture(Texture2D texture);
static inline mosaico_renderer_texture_t mosaico_2d_to_renderer_texture(
    Texture2D texture)
{
    return (mosaico_renderer_texture_t) {
        .id = texture.id, .width = texture.width, .height = texture.height,
    };
}

static inline mosaico_renderer_rect_t mosaico_2d_to_renderer_rect(
    Rectangle rectangle)
{
    return (mosaico_renderer_rect_t) {
        rectangle.x, rectangle.y, rectangle.width, rectangle.height,
    };
}

static inline mosaico_renderer_vec2_t mosaico_2d_to_renderer_vec2(
    Vector2 vector)
{
    return (mosaico_renderer_vec2_t){vector.x, vector.y};
}

static inline mosaico_renderer_color_t mosaico_2d_to_renderer_color(Color color)
{
    return (mosaico_renderer_color_t){color.r, color.g, color.b, color.a};
}

static inline void Mosaico2DDrawTexturePro(Texture2D texture, Rectangle source,
    Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    mosaico_renderer_draw_texture_pro(
        mosaico_2d_to_renderer_texture(texture),
        mosaico_2d_to_renderer_rect(source),
        mosaico_2d_to_renderer_rect(dest),
        mosaico_2d_to_renderer_vec2(origin), rotation,
        mosaico_2d_to_renderer_color(tint));
}

static inline void Mosaico2DDrawTexturedTriangle(Texture2D texture,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, unsigned light256)
{
    mosaico_renderer_draw_textured_triangle(
        mosaico_2d_to_renderer_texture(texture), a, b, c, light256);
}

static inline void Mosaico2DDrawTexturedQuad(Texture2D texture,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, mosaico_textured_vertex_t d,
    unsigned light256)
{
    mosaico_renderer_draw_textured_quad(
        mosaico_2d_to_renderer_texture(texture), a, b, c, d, light256);
}

static inline void Mosaico2DDrawTileRow(Texture2D texture,
    const uint16_t *tile_ids, size_t tile_count, int tile_width,
    int tile_height, int dest_x, int dest_y)
{
    mosaico_renderer_draw_tile_row(mosaico_2d_to_renderer_texture(texture),
        tile_ids, tile_count, tile_width, tile_height, dest_x, dest_y);
}

static inline void Mosaico2DDrawColumn(Texture2D texture, Rectangle source,
    int dest_x, int dest_y, int dest_width, int dest_height,
    unsigned light256)
{
    mosaico_renderer_draw_column(mosaico_2d_to_renderer_texture(texture),
        mosaico_2d_to_renderer_rect(source), dest_x, dest_y,
        dest_width, dest_height, light256);
}

static inline void Mosaico2DDrawSpan(Texture2D texture, Rectangle source,
    int dest_y, int dest_x0, int dest_x1, int u_16, int v_16,
    int du_16, int dv_16, unsigned light256)
{
    mosaico_renderer_draw_span(mosaico_2d_to_renderer_texture(texture),
        mosaico_2d_to_renderer_rect(source), dest_y, dest_x0, dest_x1,
        u_16, v_16, du_16, dv_16, light256);
}

static inline void Mosaico2DDrawFloorRow(Texture2D texture, Rectangle source,
    int dest_y, int dest_x, int column_width, int columns,
    const uint16_t *wall_bottom, int u_16, int v_16, int du_16,
    int dv_16, unsigned light256)
{
    mosaico_renderer_draw_floor_row(mosaico_2d_to_renderer_texture(texture),
        mosaico_2d_to_renderer_rect(source), dest_y, dest_x,
        column_width, columns, wall_bottom, u_16, v_16, du_16, dv_16,
        light256);
}

static inline void Mosaico2DDrawFloorRows(Texture2D texture, Rectangle source,
    int dest_y, int dest_x, int column_width, int columns,
    const uint16_t *wall_bottom, int u_16, int v_16, int du_16,
    int dv_16, unsigned light256, int row_repeat)
{
    mosaico_renderer_draw_floor_rows(mosaico_2d_to_renderer_texture(texture),
        mosaico_2d_to_renderer_rect(source), dest_y, dest_x,
        column_width, columns, wall_bottom, u_16, v_16, du_16, dv_16,
        light256, row_repeat);
}

static inline void Mosaico2DDrawRaycastWalls(Texture2D texture,
    const mosaico_raycast_wall_t *columns, int column_count)
{
    mosaico_renderer_draw_raycast_walls(
        mosaico_2d_to_renderer_texture(texture), columns, column_count);
}

#ifdef __cplusplus
}
#endif
