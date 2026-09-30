// SPDX-License-Identifier: Apache-2.0
#include "vg_asset.h"

#include <string.h>

void vg_asset_pose(const vg_asset_t *asset, const vg_bone_t *bones, vg_mat_t root,
                   vg_mat_t *out)
{
    /* svg2vg rejects cycles; parents may still come after children in draw order. */
    uint8_t done[256];
    int count = asset->part_count < 256 ? asset->part_count : 256;
    memset(done, 0, sizeof(done));
    for (int resolved = 0; resolved < count;) {
        int progress = 0;
        for (int i = 0; i < count; ++i) {
            if (done[i]) continue;
            const vg_asset_part_t *p = &asset->parts[i];
            vg_mat_t m;
            if (p->parent < 0) {
                m = vg_translate(root, p->pivot_x, p->pivot_y);
            } else {
                if (!done[p->parent]) continue;
                const vg_asset_part_t *pp = &asset->parts[p->parent];
                m = vg_translate(out[p->parent], p->pivot_x - pp->pivot_x,
                                 p->pivot_y - pp->pivot_y);
            }
            if (bones) {
                const vg_bone_t *b = &bones[i];
                m = vg_translate(m, b->x, b->y);
                if (b->rotation != 0) m = vg_rotate(m, b->rotation);
                if (b->sx != 1 || b->sy != 1) m = vg_scale(m, b->sx, b->sy);
            }
            out[i] = m;
            done[i] = 1;
            ++resolved;
            ++progress;
        }
        if (!progress) break;
    }
}

void vg_asset_draw_part(const vg_asset_t *asset, int part, vg_mat_t m)
{
    if (part < 0 || part >= asset->part_count) return;
    const vg_asset_part_t *p = &asset->parts[part];
    const float k = asset->coord_scale;
    vg_set_matrix(m);
    for (int s = 0; s < p->shape_count; ++s) {
        const vg_asset_shape_t *shape = &asset->shapes[p->first_shape + s];
        const uint8_t *op = asset->ops + shape->first_op;
        const int16_t *c = asset->coords + shape->first_coord;
        if (!shape->stroke) {
            vg_path_begin();
            for (int i = 0; i < shape->op_count; ++i) {
                switch (op[i]) {
                case VG_OP_MOVE: vg_move_to(c[0] * k, c[1] * k); c += 2; break;
                case VG_OP_LINE: vg_line_to(c[0] * k, c[1] * k); c += 2; break;
                case VG_OP_CUBIC:
                    vg_cubic_to(c[0] * k, c[1] * k, c[2] * k, c[3] * k, c[4] * k, c[5] * k);
                    c += 6;
                    break;
                default: vg_close(); break;
                }
            }
            vg_fill(shape->color, shape->alpha);
            continue;
        }
        float x = 0, y = 0, sx = 0, sy = 0;
        const float w = shape->width;
        for (int i = 0; i < shape->op_count; ++i) {
            switch (op[i]) {
            case VG_OP_MOVE:
                x = sx = c[0] * k;
                y = sy = c[1] * k;
                c += 2;
                break;
            case VG_OP_LINE:
                vg_stroke_line(x, y, c[0] * k, c[1] * k, w, shape->color, shape->alpha);
                x = c[0] * k;
                y = c[1] * k;
                c += 2;
                break;
            case VG_OP_CUBIC:
                vg_stroke_cubic(x, y, c[0] * k, c[1] * k, c[2] * k, c[3] * k, c[4] * k,
                                c[5] * k, w, w, shape->color, shape->alpha);
                x = c[4] * k;
                y = c[5] * k;
                c += 6;
                break;
            default:
                vg_stroke_line(x, y, sx, sy, w, shape->color, shape->alpha);
                x = sx;
                y = sy;
                break;
            }
        }
    }
}

void vg_asset_draw_range(const vg_asset_t *asset, const vg_mat_t *mats,
                         const uint8_t *visible, int first, int last)
{
    if (first < 0) first = 0;
    if (last > asset->part_count) last = asset->part_count;
    for (int i = first; i < last; ++i)
        if (!visible || visible[i]) vg_asset_draw_part(asset, i, mats[i]);
    vg_set_matrix(vg_identity());
}

int vg_asset_find(const vg_asset_t *asset, const char *name)
{
    for (int i = 0; i < asset->part_count; ++i)
        if (strcmp(asset->parts[i].name, name) == 0) return i;
    return -1;
}
