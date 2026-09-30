// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

#include "vg_raster.h"

/*
 * Layered vector artwork produced by tools/svg2vg.py. Each part is one layer
 * of the source file, stored relative to its pivot so a bone transform can
 * rotate it about the joint. Parts draw in document order.
 */

enum { VG_OP_MOVE, VG_OP_LINE, VG_OP_CUBIC, VG_OP_CLOSE };

typedef struct {
    uint16_t color;
    uint8_t alpha;
    uint8_t stroke;
    float width;
    uint32_t first_op;
    uint16_t op_count;
    uint32_t first_coord;
} vg_asset_shape_t;

typedef struct {
    const char *name;
    int16_t parent;
    float pivot_x, pivot_y;
    uint16_t first_shape;
    uint16_t shape_count;
} vg_asset_part_t;

typedef struct {
    const vg_asset_part_t *parts;
    uint16_t part_count;
    const vg_asset_shape_t *shapes;
    const uint8_t *ops;
    const int16_t *coords;
    float coord_scale;
    float width, height;
} vg_asset_t;

/* Local bone transform, applied about the part pivot. */
typedef struct {
    float x, y;
    float rotation;
    float sx, sy;
} vg_bone_t;

static inline vg_bone_t vg_bone_rest(void) { return (vg_bone_t){0, 0, 0, 1, 1}; }

/*
 * Resolves the hierarchy into one matrix per part. `bones` may be NULL for the
 * rest pose. `root` maps artwork coordinates to the screen.
 */
void vg_asset_pose(const vg_asset_t *asset, const vg_bone_t *bones, vg_mat_t root,
                   vg_mat_t *out);
void vg_asset_draw_part(const vg_asset_t *asset, int part, vg_mat_t m);
/*
 * Draws parts [first, last) with their posed matrices. `visible` may be NULL;
 * otherwise parts whose entry is 0 are skipped, which is how alternate layers
 * such as `eye_closed` replace their open counterpart.
 */
void vg_asset_draw_range(const vg_asset_t *asset, const vg_mat_t *mats,
                         const uint8_t *visible, int first, int last);
int vg_asset_find(const vg_asset_t *asset, const char *name);
