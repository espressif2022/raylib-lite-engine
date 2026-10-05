// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "raylib_lite_compat.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t id;
    int width;
    int height;
} raylib_lite_renderer_texture_t;

typedef struct { float x, y; } raylib_lite_renderer_vec2_t;
typedef struct { float x, y, width, height; } raylib_lite_renderer_rect_t;
typedef struct { uint8_t r, g, b, a; } raylib_lite_renderer_color_t;

typedef struct {
    raylib_lite_asset_id_t id;
    raylib_lite_renderer_rect_t source;
    raylib_lite_renderer_vec2_t pivot;
} raylib_lite_renderer_sprite_frame_t;

typedef struct {
    raylib_lite_renderer_texture_t texture;
    const void *descriptor;
    uint16_t frame_count;
} raylib_lite_renderer_atlas_t;

typedef struct {
    const void *descriptor;
    const void *frames;
    const uint8_t *indices;
    const uint16_t *light_lut;
    uint16_t width, height, frame_count, light_levels;
    uint8_t row_major;
} raylib_lite_wall_atlas_t;

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
} raylib_lite_renderer_raster_stats_t;

typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    int src_x, src_y, src_width, src_height;
    unsigned light256;
    int v_phase_16, v_step_16;
} raylib_lite_raycast_wall_t;

typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    uint16_t color565;
} raylib_lite_solid_wall_t;

typedef struct { float x, y, u, v, q; } raylib_lite_textured_vertex_t;

void raylib_lite_renderer_note_primitives(uint32_t pixels, uint32_t runs,
                                     uint32_t clear_pixels);
void raylib_lite_renderer_run_benchmark(void);
void raylib_lite_renderer_set_target(uint16_t *pixels, size_t stride,
                                int width, int height);
void raylib_lite_renderer_set_clip(int x, int y, int width, int height);
void raylib_lite_renderer_reset_raster_stats(void);
void raylib_lite_renderer_get_raster_stats(raylib_lite_renderer_raster_stats_t *out_stats);
uint32_t raylib_lite_renderer_get_rejected_draw_calls(void);
void raylib_lite_renderer_set_phase_us(uint32_t sky_us, uint32_t floor_us,
                                  uint32_t wall_us, uint32_t enemy_us,
                                  uint32_t hud_us);

raylib_lite_renderer_texture_t raylib_lite_renderer_load_texture(const char *asset_path);
raylib_lite_renderer_texture_t raylib_lite_renderer_register_rgb565(
    const void *pixels, int width, int height);
bool raylib_lite_renderer_cache_texture_light(raylib_lite_renderer_texture_t texture,
                                          unsigned light256);
void raylib_lite_renderer_unload_texture(raylib_lite_renderer_texture_t texture);
void raylib_lite_renderer_draw_texture_pro(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, raylib_lite_renderer_rect_t dest,
    raylib_lite_renderer_vec2_t origin, float rotation,
    raylib_lite_renderer_color_t tint);
void raylib_lite_renderer_draw_textured_triangle(raylib_lite_renderer_texture_t texture,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, unsigned light256);
void raylib_lite_renderer_draw_textured_quad(raylib_lite_renderer_texture_t texture,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, raylib_lite_textured_vertex_t d,
    unsigned light256);
void raylib_lite_renderer_draw_tile_row(raylib_lite_renderer_texture_t texture,
    const uint16_t *tile_ids, size_t tile_count, int tile_width,
    int tile_height, int dest_x, int dest_y);
void raylib_lite_renderer_draw_column(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, int dest_x, int dest_y,
    int dest_width, int dest_height, unsigned light256);
void raylib_lite_renderer_draw_span(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, int dest_y, int dest_x0, int dest_x1,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256);
void raylib_lite_renderer_draw_floor_row(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, int dest_y, int dest_x,
    int column_width, int columns, const uint16_t *wall_bottom,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256);
void raylib_lite_renderer_draw_floor_rows(raylib_lite_renderer_texture_t texture,
    raylib_lite_renderer_rect_t source, int dest_y, int dest_x,
    int column_width, int columns, const uint16_t *wall_bottom,
    int u_16, int v_16, int du_16, int dv_16, unsigned light256,
    int row_repeat);
void raylib_lite_renderer_draw_raycast_walls(raylib_lite_renderer_texture_t texture,
    const raylib_lite_raycast_wall_t *columns, int column_count);

raylib_lite_renderer_atlas_t raylib_lite_renderer_load_atlas(const char *asset_path);
const raylib_lite_renderer_sprite_frame_t *raylib_lite_renderer_atlas_get_frame(
    raylib_lite_renderer_atlas_t atlas, raylib_lite_asset_id_t frame_id);
raylib_lite_result_t raylib_lite_renderer_atlas_get_frame_copy(
    raylib_lite_renderer_atlas_t atlas, raylib_lite_asset_id_t frame_id,
    raylib_lite_renderer_sprite_frame_t *out_frame);
void raylib_lite_renderer_unload_atlas(raylib_lite_renderer_atlas_t atlas);

raylib_lite_wall_atlas_t raylib_lite_wall_atlas_load(const char *asset_path);
raylib_lite_result_t raylib_lite_renderer_wall_atlas_get_frame(raylib_lite_wall_atlas_t atlas,
    raylib_lite_asset_id_t frame_id, raylib_lite_renderer_sprite_frame_t *out_frame);
void raylib_lite_wall_atlas_unload(raylib_lite_wall_atlas_t atlas);
raylib_lite_asset_id_t raylib_lite_animation_frame_at(const raylib_lite_asset_id_t *frames,
    size_t frame_count, uint32_t frame_ticks, uint32_t elapsed_ticks, bool loop);

void raylib_lite_2d_draw_indexed_textured_triangle(raylib_lite_wall_atlas_t atlas,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, unsigned light256);
void raylib_lite_2d_draw_indexed_textured_quad(raylib_lite_wall_atlas_t atlas,
    raylib_lite_textured_vertex_t a, raylib_lite_textured_vertex_t b,
    raylib_lite_textured_vertex_t c, raylib_lite_textured_vertex_t d,
    unsigned light256);
void raylib_lite_2d_copy_scanline(int src_y, int dst_y);
void raylib_lite_2d_draw_indexed_raycast_walls(raylib_lite_wall_atlas_t atlas,
    const raylib_lite_raycast_wall_t *columns, int column_count);
void raylib_lite_2d_draw_solid_raycast_walls(const raylib_lite_solid_wall_t *columns,
                                    int column_count);

#ifdef __cplusplus
}
#endif
