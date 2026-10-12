// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Renderer instrumentation, not a per-task context. Calls must be serialized
 * with drawing. Field layout and legacy phase labels are ABI-compatible. */
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

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

void raylib_lite_renderer_note_primitives(uint32_t pixels, uint32_t runs,
                                     uint32_t clear_pixels);
void raylib_lite_renderer_reset_raster_stats(void);
void raylib_lite_renderer_get_raster_stats(raylib_lite_renderer_raster_stats_t *out_stats);
uint32_t raylib_lite_renderer_get_rejected_draw_calls(void);
void raylib_lite_renderer_set_phase_us(uint32_t sky_us, uint32_t floor_us,
                                  uint32_t wall_us, uint32_t enemy_us,
                                  uint32_t hud_us);

/* reset also clears rejected-draw counts. get copies the current snapshot;
 * note_primitives is an integration hook for external primitive writers.
 * sky/floor/wall/enemy/hud are legacy phase slots; applications may map their
 * own phases to them, but must keep that mapping explicit in their reports. */
#ifdef __cplusplus
}
#endif
