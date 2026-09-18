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
} mosaico_game_2d_raster_stats_t;
typedef struct {
    int dest_x, dest_y, dest_width, dest_height;
    int src_x, src_y, src_width, src_height;
    unsigned light256;
} mosaico_raycast_wall_t;
typedef struct { float x,y,u,v; } mosaico_textured_vertex_t;
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
mosaico_asset_id_t MosaicoAnimationFrameAt(const mosaico_asset_id_t *frames,
    size_t frame_count,uint32_t frame_ticks,uint32_t elapsed_ticks,bool loop);
void UnloadMosaicoAtlas(MosaicoAtlas atlas);
Texture2D Mosaico2DLoadTexture(const char *asset_path);
/* Register caller-owned native RGB565 pixels without copying them. The pixel
 * buffer must remain valid until Mosaico2DUnloadTexture() is called. */
Texture2D Mosaico2DRegisterRGB565(const void *pixels,int width,int height);
void Mosaico2DUnloadTexture(Texture2D texture);
void Mosaico2DDrawTexturePro(Texture2D texture,Rectangle source,Rectangle dest,Vector2 origin,float rotation,Color tint);
void Mosaico2DDrawTexturedTriangle(Texture2D texture,
    mosaico_textured_vertex_t a,mosaico_textured_vertex_t b,
    mosaico_textured_vertex_t c,unsigned light256);
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
void Mosaico2DDrawTileRow(Texture2D texture,const uint16_t *tile_ids,
    size_t tile_count,int tile_width,int tile_height,int dest_x,int dest_y);
#ifdef __cplusplus
}
#endif
