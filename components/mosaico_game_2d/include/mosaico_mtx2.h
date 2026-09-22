// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MTX2 block textures: 4x4 texels per 8-byte block, a quarter of raw RGB565.
 *
 *     uint16 c0, uint16 c1, uint32 indices   (2 bits per texel, row-major)
 *
 *     c0 >  c1  opaque:        p0=c0, p1=c1, p2=(2c0+c1)/3, p3=(c0+2c1)/3
 *     c0 <= c1  punch-through: p0=c0, p1=c1, p2=(c0+c1)/2,  p3=transparent
 *
 * Channel interpolation happens in 5/6/5 space with integer division so the
 * sampler never leaves RGB565. tools/mtx2_codec.py encodes with the exact same
 * arithmetic; changing one side without the other desynchronises decoding.
 *
 * A span costs one palette build per 4 texels at 1:1 magnification and a
 * 2-bit index lookup per pixel. Lighting is folded into the palette, so the
 * per-pixel path never shades.
 *
 * Invariant: when MOSAICO_MTX2_FLAG_OPAQUE is set every block must encode
 * c0 > c1 strictly. A flat block therefore cannot store equal endpoints; the
 * encoder emits c1 < c0 and selects index 0. The sampler's unscaled fast path
 * relies on this to drop the per-pixel transparency test.
 */

#define MOSAICO_MTX2_MAGIC 0x3258544dU /* "MTX2" */
#define MOSAICO_MTX2_FLAG_BINARY_ALPHA (1U << 0)
#define MOSAICO_MTX2_FLAG_OPAQUE (1U << 1)
#define MOSAICO_MTX2_HEADER_BYTES 20U
#define MOSAICO_MTX2_BLOCK_BYTES 8U

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, flags, mip_levels, reserved;
    uint32_t block_bytes;
} mosaico_mtx2_header_t;

typedef struct {
    const uint8_t *blocks; /* mip 0 payload, 4-byte aligned */
    int width, height;
    int block_width, block_height;
    bool punch_through;
} mosaico_mtx2_t;

/* Decode one block into a lit RGB565 palette. Returns true when the block is
 * punch-through, in which case entry 3 must not be stored. */
bool mosaico_mtx2_palette(const uint8_t *block, unsigned light256, uint16_t out_palette[4]);

/* Affine span with a constant V, the case triangle scanlines hit almost
 * always. v_texel is already in texels. U must stay inside the texture for the
 * whole run, the same precondition the RGB565 direct span fillers carry. */
void mosaico_mtx2_span_constv(uint16_t *dst, const mosaico_mtx2_t *texture,
                              int32_t u_16, int32_t du_16, int v_texel,
                              int count, unsigned light256);

/* Decode block_count blocks of one block row into lit RGB565 palettes,
 * 4 entries per block. */
void mosaico_mtx2_decode_block_row(const mosaico_mtx2_t *texture, int block_y,
                                   int first_block, int block_count,
                                   unsigned light256, uint16_t *palettes);

/* Unscaled rect blit. Decodes each block row once and reuses it for the four
 * scanlines the row covers, which is where the sampler is cheapest. Opaque
 * textures only; the source rect must lie inside the texture. */
void mosaico_mtx2_blit(uint16_t *dst, int dst_stride, const mosaico_mtx2_t *texture,
                       int u0, int v0, int width, int height, unsigned light256);

/* Affine span with V varying across the run. */
void mosaico_mtx2_span(uint16_t *dst, const mosaico_mtx2_t *texture,
                       int32_t u_16, int32_t v_16, int32_t du_16, int32_t dv_16,
                       int count, unsigned light256);

/* Parse a header-prefixed MTX2 blob. Returns false when the payload is short
 * or malformed. */
bool mosaico_mtx2_open(const void *data, size_t size, mosaico_mtx2_t *out_texture);
