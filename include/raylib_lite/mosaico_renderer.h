// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_compat.h"
#include "mosaico_game_assets.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    int width;
    int height;
} mosaico_renderer_texture_t;

typedef struct { float x, y; } mosaico_renderer_vec2_t;
typedef struct { float x, y, width, height; } mosaico_renderer_rect_t;
typedef struct { uint8_t r, g, b, a; } mosaico_renderer_color_t;

typedef struct {
    mosaico_asset_id_t id;
    mosaico_renderer_rect_t source;
    mosaico_renderer_vec2_t pivot;
} mosaico_renderer_sprite_frame_t;

typedef struct {
    mosaico_renderer_texture_t texture;
    const void *descriptor;
    uint16_t frame_count;
} mosaico_renderer_atlas_t;

typedef struct {
    const void *descriptor;
    const void *frames;
    const uint8_t *indices;
    const uint16_t *light_lut;
    uint16_t width, height, frame_count, light_levels;
    uint8_t row_major;
} MosaicoWallAtlas;

typedef struct {
    uint32_t opaque_copy_calls;
    uint32_t opaque_copy_pixels;
    uint32_t opaque_scale_calls;
    uint32_t opaque_scale_pixels;
    uint32_t binary_alpha_calls;
    uint32_t binary_alpha_pixels;
    uint32_t binary_copy_calls;
    uint32_t binary_copy_pixels;
    uint32_t binary_scale_calls;
    uint32_t binary_scale_pixels;
    uint32_t tile_row_calls;
    uint32_t tile_row_pixels;
    uint32_t alpha_calls;
    uint32_t alpha_pixels;
    uint32_t rotated_calls;
    uint32_t rotated_pixels;
    uint32_t frame_lookup_hits;
    uint32_t frame_lookup_misses;
    uint32_t column_calls;
    uint32_t column_pixels;
    uint32_t span_calls;
    uint32_t span_pixels;
    uint32_t sky_us;
    uint32_t floor_us;
    uint32_t wall_us;
    uint32_t enemy_us;
    uint32_t hud_us;
    uint32_t triangle_calls;
    uint32_t triangle_pixels;
    uint32_t triangle_direct_pixels;
    uint32_t triangle_mirror_pixels;
    uint32_t quad_calls;
    uint32_t quad_pixels;
    uint32_t primitive_pixels, primitive_runs, clear_pixels;
    uint32_t rgb_const_v_pixels, rgb_vary_v_pixels;
    uint32_t indexed_const_v_pixels, indexed_vary_v_pixels;
    uint32_t indexed_magnify_pixels, indexed_minify_pixels;
    uint32_t triangle_setup_us, triangle_raster_us;
    uint32_t fb_runs;
    uint32_t fb_pixels;
} mosaico_game_2d_raster_stats_t;

typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    int src_x, src_y, src_width, src_height;
    unsigned light256;
    int v_phase_16, v_step_16;
} mosaico_raycast_wall_t;

typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    uint16_t color565;
} mosaico_solid_wall_t;

typedef struct { float x, y, u, v, q; } mosaico_textured_vertex_t;

void mosaico_game_2d_note_primitives(uint32_t pixels, uint32_t runs,
                                     uint32_t clear_pixels);
void mosaico_game_2d_run_benchmark(void);
void mosaico_game_2d_set_target(uint16_t *pixels, size_t stride,
                                int width, int height);
void mosaico_game_2d_set_clip(int x, int y, int width, int height);
void mosaico_game_2d_reset_raster_stats(void);
void mosaico_game_2d_get_raster_stats(mosaico_game_2d_raster_stats_t *out_stats);
uint32_t mosaico_game_2d_get_rejected_draw_calls(void);
void mosaico_game_2d_set_phase_us(uint32_t sky_us, uint32_t floor_us,
                                  uint32_t wall_us, uint32_t enemy_us,
                                  uint32_t hud_us);

mosaico_renderer_texture_t mosaico_renderer_load_texture(const char *asset_path);
mosaico_renderer_texture_t mosaico_renderer_register_rgb565(
    const void *pixels, int width, int height);
bool mosaico_renderer_cache_texture_light(mosaico_renderer_texture_t texture,
                                          unsigned light256);
void mosaico_renderer_unload_texture(mosaico_renderer_texture_t texture);
void mosaico_renderer_draw_texture_pro(mosaico_renderer_texture_t texture,
    mosaico_renderer_rect_t source, mosaico_renderer_rect_t dest,
    mosaico_renderer_vec2_t origin, float rotation,
    mosaico_renderer_color_t tint);
void mosaico_renderer_draw_textured_triangle(mosaico_renderer_texture_t texture,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, unsigned light256);
void mosaico_renderer_draw_textured_quad(mosaico_renderer_texture_t texture,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, mosaico_textured_vertex_t d,
    unsigned light256);
void mosaico_renderer_draw_tile_row(mosaico_renderer_texture_t texture,
    const uint16_t *tile_ids, size_t tile_count, int tile_width,
    int tile_height, int dest_x, int dest_y);
void mosaico_renderer_draw_column(mosaico_renderer_texture_t texture,
    mosaico_renderer_rect_t source, int dest_x, int dest_y,
    int dest_width, int dest_height, unsigned light256);
void mosaico_renderer_draw_span(mosaico_renderer_texture_t texture,
    mosaico_renderer_rect_t source, int dest_y, int dest_x0, int dest_x1,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256);
void mosaico_renderer_draw_floor_row(mosaico_renderer_texture_t texture,
    mosaico_renderer_rect_t source, int dest_y, int dest_x,
    int column_width, int columns, const uint16_t *wall_bottom,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256);
void mosaico_renderer_draw_floor_rows(mosaico_renderer_texture_t texture,
    mosaico_renderer_rect_t source, int dest_y, int dest_x,
    int column_width, int columns, const uint16_t *wall_bottom,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256,
    int row_repeat);
void mosaico_renderer_draw_raycast_walls(mosaico_renderer_texture_t texture,
    const mosaico_raycast_wall_t *columns, int column_count);

mosaico_renderer_atlas_t mosaico_renderer_load_atlas(const char *asset_path);
const mosaico_renderer_sprite_frame_t *mosaico_renderer_atlas_get_frame(
    mosaico_renderer_atlas_t atlas, mosaico_asset_id_t frame_id);
esp_err_t mosaico_renderer_atlas_get_frame_copy(
    mosaico_renderer_atlas_t atlas, mosaico_asset_id_t frame_id,
    mosaico_renderer_sprite_frame_t *out_frame);
void mosaico_renderer_unload_atlas(mosaico_renderer_atlas_t atlas);

MosaicoWallAtlas LoadMosaicoWallAtlas(const char *asset_path);
esp_err_t mosaico_renderer_wall_atlas_get_frame(MosaicoWallAtlas atlas,
    mosaico_asset_id_t frame_id, mosaico_renderer_sprite_frame_t *out_frame);
void UnloadMosaicoWallAtlas(MosaicoWallAtlas atlas);
mosaico_asset_id_t MosaicoAnimationFrameAt(const mosaico_asset_id_t *frames,
    size_t frame_count, uint32_t frame_ticks, uint32_t elapsed_ticks, bool loop);

void Mosaico2DDrawIndexedTexturedTriangle(MosaicoWallAtlas atlas,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, unsigned light256);
void Mosaico2DDrawIndexedTexturedQuad(MosaicoWallAtlas atlas,
    mosaico_textured_vertex_t a, mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c, mosaico_textured_vertex_t d,
    unsigned light256);
void Mosaico2DCopyScanline(int src_y, int dst_y);
void Mosaico2DDrawIndexedRaycastWalls(MosaicoWallAtlas atlas,
    const mosaico_raycast_wall_t *columns, int column_count);
void Mosaico2DDrawSolidRaycastWalls(const mosaico_solid_wall_t *columns,
                                    int column_count);

#ifdef __cplusplus
}
#endif
