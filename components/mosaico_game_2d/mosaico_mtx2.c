// SPDX-License-Identifier: Apache-2.0
#include "mosaico_mtx2.h"

#include "mosaico_rgb565.h"

#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define MTX2_HOT IRAM_ATTR
#else
#define MTX2_HOT
#endif

/* Explicit little-endian loads: the payload is only guaranteed 4-byte aligned
 * and may live in memory-mapped flash. Both targets are little-endian, so
 * compilers fold these back into single loads. */
static inline uint16_t load16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t pack565(unsigned r, unsigned g, unsigned b)
{
    return (uint16_t)((r << 11) | (g << 5) | b);
}

bool mosaico_mtx2_palette(const uint8_t *block, unsigned light256, uint16_t out_palette[4])
{
    const uint16_t c0 = load16(block);
    const uint16_t c1 = load16(block + 2);
    const unsigned r0 = (c0 >> 11) & 31U, g0 = (c0 >> 5) & 63U, b0 = c0 & 31U;
    const unsigned r1 = (c1 >> 11) & 31U, g1 = (c1 >> 5) & 63U, b1 = c1 & 31U;
    const bool punch = c0 <= c1;

    uint16_t p2, p3;
    if (punch) {
        p2 = pack565((r0 + r1) >> 1, (g0 + g1) >> 1, (b0 + b1) >> 1);
        p3 = 0;
    } else {
        p2 = pack565((2U * r0 + r1) / 3U, (2U * g0 + g1) / 3U, (2U * b0 + b1) / 3U);
        p3 = pack565((r0 + 2U * r1) / 3U, (g0 + 2U * g1) / 3U, (b0 + 2U * b1) / 3U);
    }

    /* Fold lighting in once per block instead of once per pixel. */
    if (light256 >= 256U) {
        out_palette[0] = c0;
        out_palette[1] = c1;
        out_palette[2] = p2;
        out_palette[3] = p3;
    } else {
        out_palette[0] = mosaico_shade565(c0, light256);
        out_palette[1] = mosaico_shade565(c1, light256);
        out_palette[2] = mosaico_shade565(p2, light256);
        out_palette[3] = punch ? 0 : mosaico_shade565(p3, light256);
    }
    return punch;
}

bool mosaico_mtx2_open(const void *data, size_t size, mosaico_mtx2_t *out_texture)
{
    if (!data || !out_texture || size < MOSAICO_MTX2_HEADER_BYTES) {
        return false;
    }
    const mosaico_mtx2_header_t *header = (const mosaico_mtx2_header_t *)data;
    if (header->magic != MOSAICO_MTX2_MAGIC || !header->width || !header->height) {
        return false;
    }
    const size_t frames = (size_t)header->frame_count * 16U;
    const size_t offset = MOSAICO_MTX2_HEADER_BYTES + frames;
    const int block_width = (header->width + 3) >> 2;
    const int block_height = (header->height + 3) >> 2;
    const size_t level0 = (size_t)block_width * (size_t)block_height * MOSAICO_MTX2_BLOCK_BYTES;
    if (offset > size || size - offset < level0 || header->block_bytes < level0) {
        return false;
    }
    *out_texture = (mosaico_mtx2_t){
        .blocks = (const uint8_t *)data + offset,
        .width = header->width,
        .height = header->height,
        .block_width = block_width,
        .block_height = block_height,
        .punch_through = (header->flags & MOSAICO_MTX2_FLAG_OPAQUE) == 0U,
    };
    return true;
}

MTX2_HOT void mosaico_mtx2_span_constv(uint16_t *dst, const mosaico_mtx2_t *texture,
                                       int32_t u_16, int32_t du_16, int v_texel,
                                       int count, unsigned light256)
{
    if (count <= 0) {
        return;
    }
    if (v_texel < 0) {
        v_texel = 0;
    } else if (v_texel >= texture->height) {
        v_texel = texture->height - 1;
    }
    const uint8_t *row = texture->blocks
                       + (size_t)(v_texel >> 2) * (size_t)texture->block_width
                             * MOSAICO_MTX2_BLOCK_BYTES;
    /* Bit position of this texel row inside the block's index word. */
    const unsigned row_shift = (unsigned)(v_texel & 3) * 8U;

    int cached = -1;
    uint32_t bits = 0;
    uint16_t palette[4] = {0, 0, 0, 0};

    /* Unscaled 1:1 blits walk texels consecutively, so a whole block row can
     * be emitted per palette build. The index layout puts each texel row in
     * its own byte of the index word, which makes the run a single byte load
     * and four constant-shift extracts with no per-pixel block test. */
    if (du_16 == (1 << 16) && !texture->punch_through) {
        const uint8_t *index_byte = row + 4 + (size_t)(v_texel & 3);
        int x = u_16 >> 16;
        int i = 0;
        while (i < count) {
            const int sub = x & 3;
            const uint8_t *block = row + (size_t)(x >> 2) * MOSAICO_MTX2_BLOCK_BYTES;
            mosaico_mtx2_palette(block, light256, palette);
            const unsigned idx = index_byte[(size_t)(x >> 2) * MOSAICO_MTX2_BLOCK_BYTES];
            int run = 4 - sub;
            if (run > count - i) {
                run = count - i;
            }
            for (int k = 0; k < run; ++k) {
                dst[i + k] = palette[(idx >> (2U * (unsigned)(sub + k))) & 3U];
            }
            i += run;
            x += run;
        }
        return;
    }

    /* Opaque textures take a loop with no per-pixel transparency branch. U is
     * a precondition of the caller here, exactly as for the RGB565 direct
     * fillers, so the inner loop carries no clamping either. */
    if (!texture->punch_through) {
        for (int i = 0; i < count; ++i) {
            const int x = u_16 >> 16;
            const int bx = x >> 2;
            if (bx != cached) {
                const uint8_t *block = row + (size_t)bx * MOSAICO_MTX2_BLOCK_BYTES;
                mosaico_mtx2_palette(block, light256, palette);
                bits = load32(block + 4);
                cached = bx;
            }
            dst[i] = palette[(bits >> (row_shift + 2U * (unsigned)(x & 3))) & 3U];
            u_16 += du_16;
        }
        return;
    }

    bool punch = false;
    for (int i = 0; i < count; ++i) {
        const int x = u_16 >> 16;
        const int bx = x >> 2;
        if (bx != cached) {
            const uint8_t *block = row + (size_t)bx * MOSAICO_MTX2_BLOCK_BYTES;
            punch = mosaico_mtx2_palette(block, light256, palette);
            bits = load32(block + 4);
            cached = bx;
        }
        const unsigned sel = (bits >> (row_shift + 2U * (unsigned)(x & 3))) & 3U;
        if (!punch || sel != 3U) {
            dst[i] = palette[sel];
        }
        u_16 += du_16;
    }
}

MTX2_HOT void mosaico_mtx2_decode_block_row(const mosaico_mtx2_t *texture, int block_y,
                                            int first_block, int block_count,
                                            unsigned light256, uint16_t *palettes)
{
    const uint8_t *block = texture->blocks
                         + ((size_t)block_y * (size_t)texture->block_width
                            + (size_t)first_block)
                               * MOSAICO_MTX2_BLOCK_BYTES;
    for (int i = 0; i < block_count; ++i) {
        mosaico_mtx2_palette(block, light256, palettes + 4 * (size_t)i);
        block += MOSAICO_MTX2_BLOCK_BYTES;
    }
}

MTX2_HOT void mosaico_mtx2_blit(uint16_t *dst, int dst_stride, const mosaico_mtx2_t *texture,
                                int u0, int v0, int width, int height, unsigned light256)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    /* A block covers four scanlines, so decoding per block row instead of per
     * scanline cuts the palette work by four. The scratch holds one block row
     * of lit palettes; wider textures are processed in horizontal chunks. */
    enum { CHUNK_BLOCKS = 256 };
    uint16_t palettes[CHUNK_BLOCKS * 4];

    for (int x = 0; x < width;) {
        const int first_block = (u0 + x) >> 2;
        const int sub_u = (u0 + x) & 3;
        int span = width - x;
        int blocks = (sub_u + span + 3) >> 2;
        if (blocks > CHUNK_BLOCKS) {
            blocks = CHUNK_BLOCKS;
            span = blocks * 4 - sub_u;
        }

        int cached_block_y = -1;
        for (int y = 0; y < height; ++y) {
            const int v = v0 + y;
            const int block_y = v >> 2;
            if (block_y != cached_block_y) {
                mosaico_mtx2_decode_block_row(texture, block_y, first_block, blocks,
                                              light256, palettes);
                cached_block_y = block_y;
            }
            const uint8_t *index_row = texture->blocks
                                     + ((size_t)block_y * (size_t)texture->block_width
                                        + (size_t)first_block)
                                           * MOSAICO_MTX2_BLOCK_BYTES
                                     + 4 + (size_t)(v & 3);
            uint16_t *out = dst + (size_t)y * (size_t)dst_stride + (size_t)x;
            int i = 0, sub = sub_u, b = 0;
            while (i < span) {
                const unsigned idx = index_row[(size_t)b * MOSAICO_MTX2_BLOCK_BYTES];
                const uint16_t *palette = palettes + 4 * (size_t)b;
                int run = 4 - sub;
                if (run > span - i) {
                    run = span - i;
                }
                for (int k = 0; k < run; ++k) {
                    out[i + k] = palette[(idx >> (2U * (unsigned)(sub + k))) & 3U];
                }
                i += run;
                sub = 0;
                ++b;
            }
        }
        x += span;
    }
}

MTX2_HOT void mosaico_mtx2_span(uint16_t *dst, const mosaico_mtx2_t *texture,
                                int32_t u_16, int32_t v_16, int32_t du_16, int32_t dv_16,
                                int count, unsigned light256)
{
    if (count <= 0) {
        return;
    }
    if (dv_16 == 0) {
        mosaico_mtx2_span_constv(dst, texture, u_16, du_16, v_16 >> 16, count, light256);
        return;
    }
    const int max_x = texture->width - 1;
    const int max_y = texture->height - 1;

    int cached_bx = -1, cached_by = -1;
    uint32_t bits = 0;
    uint16_t palette[4] = {0, 0, 0, 0};
    bool punch = false;

    for (int i = 0; i < count; ++i) {
        int x = u_16 >> 16;
        int y = v_16 >> 16;
        x = x < 0 ? 0 : (x > max_x ? max_x : x);
        y = y < 0 ? 0 : (y > max_y ? max_y : y);
        const int bx = x >> 2, by = y >> 2;
        if (bx != cached_bx || by != cached_by) {
            const uint8_t *block = texture->blocks
                                 + ((size_t)by * (size_t)texture->block_width + (size_t)bx)
                                       * MOSAICO_MTX2_BLOCK_BYTES;
            punch = mosaico_mtx2_palette(block, light256, palette);
            bits = load32(block + 4);
            cached_bx = bx;
            cached_by = by;
        }
        const unsigned sel = (bits >> ((unsigned)(y & 3) * 8U + 2U * (unsigned)(x & 3))) & 3U;
        if (!punch || sel != 3U) {
            dst[i] = palette[sel];
        }
        u_16 += du_16;
        v_16 += dv_16;
    }
}
