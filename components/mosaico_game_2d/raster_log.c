// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_2d.h"
#include "esp_log.h"
void mosaico_game_2d_log_raster_shape(const char*tag){
 mosaico_game_2d_raster_stats_t s_raster_stats;
 mosaico_game_2d_get_raster_stats(&s_raster_stats);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster fb_runs=%lu fb_pixels=%lu tris=%lu/%lu quads=%lu/%lu",
  (unsigned long)s_raster_stats.fb_runs,(unsigned long)s_raster_stats.fb_pixels,
  (unsigned long)s_raster_stats.triangle_calls,(unsigned long)s_raster_stats.triangle_pixels,
  (unsigned long)s_raster_stats.quad_calls,(unsigned long)s_raster_stats.quad_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path opaque_copy_calls=%lu opaque_copy_pixels=%lu",
  (unsigned long)s_raster_stats.opaque_copy_calls,(unsigned long)s_raster_stats.opaque_copy_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path opaque_scale_calls=%lu opaque_scale_pixels=%lu",
  (unsigned long)s_raster_stats.opaque_scale_calls,(unsigned long)s_raster_stats.opaque_scale_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_alpha_calls=%lu binary_alpha_pixels=%lu",
  (unsigned long)s_raster_stats.binary_alpha_calls,(unsigned long)s_raster_stats.binary_alpha_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_copy_calls=%lu binary_copy_pixels=%lu",
  (unsigned long)s_raster_stats.binary_copy_calls,(unsigned long)s_raster_stats.binary_copy_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path binary_scale_calls=%lu binary_scale_pixels=%lu",
  (unsigned long)s_raster_stats.binary_scale_calls,(unsigned long)s_raster_stats.binary_scale_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path tile_row_calls=%lu tile_row_pixels=%lu",
  (unsigned long)s_raster_stats.tile_row_calls,(unsigned long)s_raster_stats.tile_row_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path alpha_calls=%lu alpha_pixels=%lu",
  (unsigned long)s_raster_stats.alpha_calls,(unsigned long)s_raster_stats.alpha_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path rotated_calls=%lu rotated_pixels=%lu",
  (unsigned long)s_raster_stats.rotated_calls,(unsigned long)s_raster_stats.rotated_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path column_calls=%lu column_pixels=%lu",
  (unsigned long)s_raster_stats.column_calls,(unsigned long)s_raster_stats.column_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path span_calls=%lu span_pixels=%lu",
  (unsigned long)s_raster_stats.span_calls,(unsigned long)s_raster_stats.span_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path frame_lookup_hits=%lu frame_lookup_misses=%lu triangle_direct_pixels=%lu triangle_mirror_pixels=%lu",
  (unsigned long)s_raster_stats.frame_lookup_hits,(unsigned long)s_raster_stats.frame_lookup_misses,
  (unsigned long)s_raster_stats.triangle_direct_pixels,(unsigned long)s_raster_stats.triangle_mirror_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path primitive_pixels=%lu primitive_runs=%lu clear_pixels=%lu rgb_const_v_pixels=%lu rgb_vary_v_pixels=%lu",
  (unsigned long)s_raster_stats.primitive_pixels,(unsigned long)s_raster_stats.primitive_runs,(unsigned long)s_raster_stats.clear_pixels,
  (unsigned long)s_raster_stats.rgb_const_v_pixels,(unsigned long)s_raster_stats.rgb_vary_v_pixels);
 ESP_LOGI(tag?tag:"mosaico_game_2d","raster_path indexed_const_v_pixels=%lu indexed_vary_v_pixels=%lu indexed_magnify_pixels=%lu indexed_minify_pixels=%lu triangle_setup_us=%lu triangle_raster_us=%lu",
  (unsigned long)s_raster_stats.indexed_const_v_pixels,(unsigned long)s_raster_stats.indexed_vary_v_pixels,
  (unsigned long)s_raster_stats.indexed_magnify_pixels,(unsigned long)s_raster_stats.indexed_minify_pixels,
  (unsigned long)s_raster_stats.triangle_setup_us,(unsigned long)s_raster_stats.triangle_raster_us);
}
