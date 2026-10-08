// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Legacy Raylib-shaped Raylib-shaped 2D facade. The raster core contract lives in
 * raylib_lite_renderer.h and does not depend on Raylib types. */
#include "raylib_lite_renderer.h"
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    raylib_lite_asset_id_t id;
    Rectangle source;
    Vector2 pivot;
} raylib_lite_sprite_frame_t;

typedef struct {
    Texture2D texture;
    const void *descriptor;
    uint16_t frame_count;
} raylib_lite_atlas_t;

raylib_lite_atlas_t raylib_lite_atlas_load(const char *asset_path);
const raylib_lite_sprite_frame_t *raylib_lite_atlas_get_frame(raylib_lite_atlas_t atlas,
                                               raylib_lite_asset_id_t frame_id);
raylib_lite_result_t raylib_lite_atlas_get_frame_copy(raylib_lite_atlas_t atlas,
    raylib_lite_asset_id_t frame_id, raylib_lite_sprite_frame_t *out_frame);
raylib_lite_result_t raylib_lite_wall_atlas_get_frame(raylib_lite_wall_atlas_t atlas,
    raylib_lite_asset_id_t frame_id, raylib_lite_sprite_frame_t *out_frame);
void raylib_lite_atlas_unload(raylib_lite_atlas_t atlas);

Texture2D raylib_lite_2d_load_texture(const char *asset_path);
Texture2D raylib_lite_2d_register_rgb565(const void *pixels, int width, int height);
bool raylib_lite_2d_cache_texture_light(Texture2D texture, unsigned light256);
void raylib_lite_2d_unload_texture(Texture2D texture);
static inline raylib_lite_renderer_texture_t raylib_lite_2d_to_renderer_texture(
    Texture2D texture)
{
    return (raylib_lite_renderer_texture_t) {
        .id = texture.id, .width = texture.width, .height = texture.height,
    };
}

static inline raylib_lite_renderer_rect_t raylib_lite_2d_to_renderer_rect(
    Rectangle rectangle)
{
    return (raylib_lite_renderer_rect_t) {
        rectangle.x, rectangle.y, rectangle.width, rectangle.height,
    };
}

static inline raylib_lite_renderer_vec2_t raylib_lite_2d_to_renderer_vec2(
    Vector2 vector)
{
    return (raylib_lite_renderer_vec2_t){vector.x, vector.y};
}

static inline raylib_lite_renderer_color_t raylib_lite_2d_to_renderer_color(Color color)
{
    return (raylib_lite_renderer_color_t){color.r, color.g, color.b, color.a};
}

static inline void raylib_lite_2d_draw_texture_pro(Texture2D texture, Rectangle source,
    Rectangle dest, Vector2 origin, float rotation, Color tint)
{
    raylib_lite_renderer_draw_texture_pro(
        raylib_lite_2d_to_renderer_texture(texture),
        raylib_lite_2d_to_renderer_rect(source),
        raylib_lite_2d_to_renderer_rect(dest),
        raylib_lite_2d_to_renderer_vec2(origin), rotation,
        raylib_lite_2d_to_renderer_color(tint));
}

static inline void raylib_lite_2d_draw_textured_triangle(Texture2D texture,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, unsigned light256)
{
    raylib_lite_renderer_draw_textured_triangle(
        raylib_lite_2d_to_renderer_texture(texture), a, b, c, light256);
}

static inline void raylib_lite_2d_draw_textured_quad(Texture2D texture,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, raylib_lite_textured_vertex_t d,
    unsigned light256)
{
    raylib_lite_renderer_draw_textured_quad(
        raylib_lite_2d_to_renderer_texture(texture), a, b, c, d, light256);
}

static inline void raylib_lite_2d_draw_tile_row(Texture2D texture,
    const uint16_t *tile_ids, size_t tile_count, int tile_width,
    int tile_height, int dest_x, int dest_y)
{
    raylib_lite_renderer_draw_tile_row(raylib_lite_2d_to_renderer_texture(texture),
        tile_ids, tile_count, tile_width, tile_height, dest_x, dest_y);
}

static inline void raylib_lite_2d_draw_column(Texture2D texture, Rectangle source,
    int dest_x, int dest_y, int dest_width, int dest_height,
    unsigned light256)
{
    raylib_lite_renderer_draw_column(raylib_lite_2d_to_renderer_texture(texture),
        raylib_lite_2d_to_renderer_rect(source), dest_x, dest_y,
        dest_width, dest_height, light256);
}

static inline void raylib_lite_2d_draw_span(Texture2D texture, Rectangle source,
    int dest_y, int dest_x0, int dest_x1, int u_16, int v_16,
    int du_16, int dv_16, unsigned light256)
{
    raylib_lite_renderer_draw_span(raylib_lite_2d_to_renderer_texture(texture),
        raylib_lite_2d_to_renderer_rect(source), dest_y, dest_x0, dest_x1,
        u_16, v_16, du_16, dv_16, light256);
}

static inline void raylib_lite_2d_draw_floor_row(Texture2D texture, Rectangle source,
    int dest_y, int dest_x, int column_width, int columns,
    const uint16_t *wall_bottom, int u_16, int v_16, int du_16,
    int dv_16, unsigned light256)
{
    raylib_lite_renderer_draw_floor_row(raylib_lite_2d_to_renderer_texture(texture),
        raylib_lite_2d_to_renderer_rect(source), dest_y, dest_x,
        column_width, columns, wall_bottom, u_16, v_16, du_16, dv_16,
        light256);
}

static inline void raylib_lite_2d_draw_floor_rows(Texture2D texture, Rectangle source,
    int dest_y, int dest_x, int column_width, int columns,
    const uint16_t *wall_bottom, int u_16, int v_16, int du_16,
    int dv_16, unsigned light256, int row_repeat)
{
    raylib_lite_renderer_draw_floor_rows(raylib_lite_2d_to_renderer_texture(texture),
        raylib_lite_2d_to_renderer_rect(source), dest_y, dest_x,
        column_width, columns, wall_bottom, u_16, v_16, du_16, dv_16,
        light256, row_repeat);
}

static inline void raylib_lite_2d_draw_raycast_walls(Texture2D texture,
    const raylib_lite_raycast_wall_t *columns, int column_count)
{
    raylib_lite_renderer_draw_raycast_walls(
        raylib_lite_2d_to_renderer_texture(texture), columns, column_count);
}

#ifdef __cplusplus
}
#endif
