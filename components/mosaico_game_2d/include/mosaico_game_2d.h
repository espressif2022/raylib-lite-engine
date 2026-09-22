// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "mosaico_game_assets.h"
#include "raylib.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { mosaico_asset_id_t id; Rectangle source; Vector2 pivot; } MosaicoSpriteFrame;
typedef struct { Texture2D texture; const void *descriptor; uint16_t frame_count; } MosaicoAtlas;
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
    /* Framebuffer store shape. A run is one contiguous horizontal write.
     * fb_pixels/fb_runs is the mean run length of instrumented paths;
     * this is not unique coverage or a measurement of cache-line traffic. */
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
    /* 0 keeps the integer row sampler. Otherwise 16.16 source V at dest_y
     * and the per-row step. One texel per row; no second tap. */
    int v_phase_16, v_step_16;
} mosaico_raycast_wall_t;
typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    uint16_t color565;
} mosaico_solid_wall_t;
/* q == 0 keeps affine u,v. q > 0 is 1/z. INDEX8 triangle and quad then
 * interpolate u*q, v*q and q, and divide only at span ends. RGB565 draws
 * ignore q. */
typedef struct { float x,y,u,v,q; } mosaico_textured_vertex_t;
void mosaico_game_2d_note_primitives(uint32_t pixels, uint32_t runs, uint32_t clear_pixels);
void mosaico_game_2d_run_benchmark(void);
void mosaico_game_2d_set_target(uint16_t *pixels,size_t stride,int width,int height);
void mosaico_game_2d_set_clip(int x,int y,int width,int height);
void mosaico_game_2d_reset_raster_stats(void);
void mosaico_game_2d_get_raster_stats(mosaico_game_2d_raster_stats_t *out_stats);
void mosaico_game_2d_set_phase_us(uint32_t sky_us, uint32_t floor_us, uint32_t wall_us,
                                  uint32_t enemy_us, uint32_t hud_us);
MosaicoAtlas LoadMosaicoAtlas(const char *asset_path);
const MosaicoSpriteFrame *MosaicoAtlasGetFrame(MosaicoAtlas atlas,mosaico_asset_id_t frame_id);
esp_err_t mosaico_game_2d_atlas_get_frame(MosaicoAtlas atlas,
    mosaico_asset_id_t frame_id,MosaicoSpriteFrame *out_frame);
MosaicoWallAtlas LoadMosaicoWallAtlas(const char *asset_path);
esp_err_t mosaico_game_2d_wall_atlas_get_frame(MosaicoWallAtlas atlas,
    mosaico_asset_id_t frame_id,MosaicoSpriteFrame *out_frame);
void UnloadMosaicoWallAtlas(MosaicoWallAtlas atlas);
mosaico_asset_id_t MosaicoAnimationFrameAt(const mosaico_asset_id_t *frames,
    size_t frame_count,uint32_t frame_ticks,uint32_t elapsed_ticks,bool loop);
void UnloadMosaicoAtlas(MosaicoAtlas atlas);
Texture2D Mosaico2DLoadTexture(const char *asset_path);
/* Register caller-owned native RGB565 pixels without copying them. The pixel
 * buffer must remain valid until Mosaico2DUnloadTexture() is called. */
Texture2D Mosaico2DRegisterRGB565(const void *pixels,int width,int height);
/* Optional exact pre-shading for triangle/quad draws at one quantized light.
 * Uses width*height*2 bytes (PSRAM on device), released on unload. Allocation
 * failure leaves ordinary sampling available. Source pixels must be immutable
 * while cached; light >= 248 drops the cache. Other draw paths are unchanged. */
bool Mosaico2DCacheTextureLight(Texture2D texture,unsigned light256);
void Mosaico2DUnloadTexture(Texture2D texture);
void Mosaico2DDrawTexturePro(Texture2D texture,Rectangle source,Rectangle dest,Vector2 origin,float rotation,Color tint);
void Mosaico2DDrawTexturedTriangle(Texture2D texture,
    mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c,unsigned light256);
/* Grid quad in Z order: a--b / c--d. Emits (a,c,b) then (b,c,d). */
void Mosaico2DDrawTexturedQuad(Texture2D texture,
    mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light256);
/* INDEX8 wall-atlas triangle/quad using the same 16-level RGB565 light LUT as
 * Mosaico2DDrawIndexedRaycastWalls. Host and device retain identical samples. */
void Mosaico2DDrawIndexedTexturedTriangle(MosaicoWallAtlas atlas,
    mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c,unsigned light256);
void Mosaico2DDrawIndexedTexturedQuad(MosaicoWallAtlas atlas,
    mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c,mosaico_textured_vertex_t d,unsigned light256);
/* Opaque-atlas raycasting primitives. Alpha atlases are intentionally rejected. */
void Mosaico2DDrawColumn(Texture2D texture, Rectangle source, int dest_x,
                         int dest_y, int dest_width, int dest_height,
                         unsigned light256);
void Mosaico2DDrawSpan(Texture2D texture, Rectangle source, int dest_y,
                       int dest_x0, int dest_x1, int u_16, int v_16,
                       int du_16, int dv_16, unsigned light256);
void Mosaico2DDrawFloorRow(Texture2D texture, Rectangle source, int dest_y,
                           int dest_x, int column_width, int columns,
                           const uint16_t *wall_bottom, int u_16, int v_16,
                           int du_16, int dv_16, unsigned light256);
void Mosaico2DDrawFloorRows(Texture2D texture, Rectangle source, int dest_y,
                            int dest_x, int column_width, int columns,
                            const uint16_t *wall_bottom, int u_16, int v_16,
                            int du_16, int dv_16, unsigned light256, int row_repeat);
void Mosaico2DCopyScanline(int src_y, int dst_y);
void Mosaico2DDrawRaycastWalls(Texture2D texture,
                               const mosaico_raycast_wall_t *columns, int column_count);
/* INDEX8 atlas path with a build-time RGB565 light LUT. MSW1 column-major is
 * optimized for ray-cast columns; MSW2 row-major is optimized for textured
 * triangle/quad spans. Host and device retain identical samples. */
void Mosaico2DDrawIndexedRaycastWalls(MosaicoWallAtlas atlas,
                                      const mosaico_raycast_wall_t *columns,
                                      int column_count);
void Mosaico2DDrawSolidRaycastWalls(const mosaico_solid_wall_t *columns,
                                    int column_count);
void Mosaico2DDrawTileRow(Texture2D texture,const uint16_t *tile_ids,
    size_t tile_count,int tile_width,int tile_height,int dest_x,int dest_y);
#ifdef __cplusplus
}
#endif
