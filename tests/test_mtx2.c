// SPDX-License-Identifier: Apache-2.0
/* MTX2 decoder oracle plus an informational sampler benchmark.
 *
 * The benchmark answers one question: does replacing a raw RGB565 sampler with
 * a block sampler cost throughput? It runs two working-set sizes on purpose.
 * The small texture stays in cache and therefore measures pure ALU cost, where
 * MTX2 is expected to lose. The large texture streams and measures the memory
 * cost that dominates on the device, where the 4x smaller footprint should win.
 * Host timings are informational and do not establish device FPS. */
#define _POSIX_C_SOURCE 200809L /* clock_gettime under -std=c11 */

#include "mosaico_mtx2.h"
#include "mosaico_rgb565.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SPAN 480

static void *read_file(const char *path, size_t *out_size)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    void *data = malloc((size_t)size);
    if (!data || fread(data, 1, (size_t)size, file) != (size_t)size) {
        fprintf(stderr, "cannot read %s\n", path);
        exit(1);
    }
    fclose(file);
    *out_size = (size_t)size;
    return data;
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Reference RGB565 sampler with the same loop shape the engine's direct span
 * fillers use, so the benchmark compares like with like. */
static void rgb565_span(uint16_t *dst, const uint16_t *row, int32_t u_16, int32_t du_16,
                        int count, unsigned light)
{
    if (light >= 256U) {
        for (int i = 0; i < count; ++i) {
            dst[i] = row[u_16 >> 16];
            u_16 += du_16;
        }
    } else {
        for (int i = 0; i < count; ++i) {
            dst[i] = mosaico_shade565(row[u_16 >> 16], light);
            u_16 += du_16;
        }
    }
}

static int check_oracle(const char *dir, const char *name, int blocks_side)
{
    char path[1024];
    size_t blob_size = 0, expect_size = 0;
    snprintf(path, sizeof(path), "%s/%s.mtx2", dir, name);
    void *blob = read_file(path, &blob_size);
    snprintf(path, sizeof(path), "%s/%s.expect", dir, name);
    int32_t *expect = read_file(path, &expect_size);

    mosaico_mtx2_t texture;
    if (!mosaico_mtx2_open(blob, blob_size, &texture)) {
        fprintf(stderr, "mosaico_mtx2_open rejected a valid blob\n");
        return 1;
    }
    const int width = blocks_side * 4, height = blocks_side * 4;
    if (texture.width != width || texture.height != height) {
        fprintf(stderr, "header mismatch: %dx%d\n", texture.width, texture.height);
        return 1;
    }

    const unsigned lights[2] = {256U, 192U};
    uint16_t *line = malloc(sizeof(uint16_t) * (size_t)width);
    int failures = 0;
    size_t cursor = 0;

    for (int l = 0; l < 2; ++l) {
        for (int y = 0; y < height; ++y) {
            /* Sentinel so skipped punch-through texels are detectable. */
            for (int x = 0; x < width; ++x) {
                line[x] = 0xDEADU;
            }
            mosaico_mtx2_span_constv(line, &texture, 0, 1 << 16, y, width, lights[l]);
            for (int x = 0; x < width; ++x) {
                int32_t want = expect[cursor + (size_t)x];
                uint16_t got = line[x];
                int ok = (want < 0) ? (got == 0xDEADU) : (got == (uint16_t)want);
                if (!ok && failures < 8) {
                    fprintf(stderr,
                            "light=%u (%d,%d): want %s%d got %u\n",
                            lights[l], x, y, want < 0 ? "transparent/" : "", want, got);
                    ++failures;
                } else if (!ok) {
                    ++failures;
                }
            }
            cursor += (size_t)width;
        }
    }

    /* The varying-V entry point must agree with the constant-V fast path. */
    for (int y = 0; y < height; ++y) {
        uint16_t a[SPAN], b[SPAN];
        int count = width < SPAN ? width : SPAN;
        for (int x = 0; x < count; ++x) {
            a[x] = b[x] = 0xDEADU;
        }
        mosaico_mtx2_span_constv(a, &texture, 0, 1 << 16, y, count, 192U);
        mosaico_mtx2_span(b, &texture, 0, y << 16, 1 << 16, 0, count, 192U);
        if (memcmp(a, b, sizeof(uint16_t) * (size_t)count) != 0) {
            fprintf(stderr, "constv and general span disagree on row %d\n", y);
            ++failures;
            break;
        }
    }

    free(line);
    free(blob);
    free(expect);
    if (failures) {
        fprintf(stderr, "%d MTX2 oracle mismatches in %s\n", failures, name);
        return 1;
    }
    printf("MTX2 oracle passed (%s): %d texels x 2 light levels\n", name, width * height);
    return 0;
}

static uint32_t rng_state = 0x1234567u;

static uint32_t next_random(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* Build an opaque MTX2 texture of the requested size plus its decoded RGB565
 * twin, so both samplers read identical content and differ only in storage. */
static void make_pair(int side, uint8_t **out_blob, size_t *out_blob_size,
                      uint16_t **out_raw, mosaico_mtx2_t *out_texture)
{
    const int blocks_side = side / 4;
    const size_t block_count = (size_t)blocks_side * (size_t)blocks_side;
    const size_t payload = block_count * MOSAICO_MTX2_BLOCK_BYTES;
    const size_t total = MOSAICO_MTX2_HEADER_BYTES + payload;

    uint8_t *blob = malloc(total);
    mosaico_mtx2_header_t header = {
        .magic = MOSAICO_MTX2_MAGIC,
        .width = (uint16_t)side,
        .height = (uint16_t)side,
        .frame_count = 0,
        .flags = MOSAICO_MTX2_FLAG_OPAQUE,
        .mip_levels = 1,
        .reserved = 0,
        .block_bytes = (uint32_t)payload,
    };
    memcpy(blob, &header, sizeof(header));
    uint8_t *blocks = blob + MOSAICO_MTX2_HEADER_BYTES;
    for (size_t i = 0; i < block_count; ++i) {
        uint32_t a = next_random() & 0xFFFFu;
        uint32_t b = next_random() & 0xFFFFu;
        uint32_t c0 = a > b ? a : b, c1 = a > b ? b : a;
        if (c0 == c1) {
            c0 = c1 + 1u > 0xFFFFu ? 0xFFFFu : c1 + 1u;
        }
        uint8_t *block = blocks + i * MOSAICO_MTX2_BLOCK_BYTES;
        block[0] = (uint8_t)c0;
        block[1] = (uint8_t)(c0 >> 8);
        block[2] = (uint8_t)c1;
        block[3] = (uint8_t)(c1 >> 8);
        uint32_t bits = next_random();
        block[4] = (uint8_t)bits;
        block[5] = (uint8_t)(bits >> 8);
        block[6] = (uint8_t)(bits >> 16);
        block[7] = (uint8_t)(bits >> 24);
    }

    if (!mosaico_mtx2_open(blob, total, out_texture)) {
        fprintf(stderr, "generated blob rejected\n");
        exit(1);
    }
    uint16_t *raw = malloc(sizeof(uint16_t) * (size_t)side * (size_t)side);
    for (int y = 0; y < side; ++y) {
        mosaico_mtx2_span_constv(raw + (size_t)y * (size_t)side, out_texture, 0, 1 << 16, y,
                                 side, 256U);
    }
    *out_blob = blob;
    *out_blob_size = total;
    *out_raw = raw;
}

static void benchmark(const char *label, int side)
{
    uint8_t *blob = NULL;
    uint16_t *raw = NULL;
    size_t blob_size = 0;
    mosaico_mtx2_t texture;
    make_pair(side, &blob, &blob_size, &raw, &texture);

    uint16_t dst[SPAN];
    const int rounds = 4000;
    /* Rows are visited in a scattered order so the prefetcher cannot hide the
     * working set, and sampling is 1:1 so neither format is penalised by
     * minification that a mip level would normally absorb. */
    int *rows = malloc(sizeof(int) * (size_t)rounds);
    for (int r = 0; r < rounds; ++r) {
        rows[r] = (int)(next_random() % (uint32_t)side);
    }

    printf("  %-11s %5dx%-5d raw %7zu KB  mtx2 %7zu KB", label, side, side,
           ((size_t)side * (size_t)side * 2) >> 10, ((size_t)side * (size_t)side / 2) >> 10);

    for (int shaded = 0; shaded < 2; ++shaded) {
        const unsigned light = shaded ? 192U : 256U;
        volatile uint16_t sink = 0;

        double t0 = now_seconds();
        for (int r = 0; r < rounds; ++r) {
            rgb565_span(dst, raw + (size_t)rows[r] * (size_t)side, 0, 1 << 16, SPAN, light);
            sink = (uint16_t)(sink + dst[0]);
        }
        double raw_ns = (now_seconds() - t0) * 1e9 / (rounds * (double)SPAN);

        t0 = now_seconds();
        for (int r = 0; r < rounds; ++r) {
            mosaico_mtx2_span_constv(dst, &texture, 0, 1 << 16, rows[r], SPAN, light);
            sink = (uint16_t)(sink + dst[0]);
        }
        double mtx2_ns = (now_seconds() - t0) * 1e9 / (rounds * (double)SPAN);

        printf("  | %-8s raw %5.2f  mtx2 %5.2f ns/px (%.2fx)",
               shaded ? "shaded" : "unshaded", raw_ns, mtx2_ns, mtx2_ns / raw_ns);
    }
    printf("\n");

    /* Same comparison for the unscaled rect blit, where MTX2 decodes each
     * block row once for the four scanlines it covers. */
    const int tile = side < 128 ? side / 2 : 64;
    uint16_t *target = malloc(sizeof(uint16_t) * (size_t)tile * (size_t)tile);
    const int blits = rounds / 4;
    printf("  %-11s blit %dx%-3d                                   ", "", tile, tile);
    for (int shaded = 0; shaded < 2; ++shaded) {
        const unsigned light = shaded ? 192U : 256U;
        volatile uint16_t sink = 0;
        const double pixels = (double)blits * tile * tile;

        double t0 = now_seconds();
        for (int r = 0; r < blits; ++r) {
            const int v0 = rows[r] % (side - tile);
            for (int y = 0; y < tile; ++y) {
                rgb565_span(target + (size_t)y * (size_t)tile,
                            raw + (size_t)(v0 + y) * (size_t)side, 0, 1 << 16, tile, light);
            }
            sink = (uint16_t)(sink + target[0]);
        }
        double raw_ns = (now_seconds() - t0) * 1e9 / pixels;

        t0 = now_seconds();
        for (int r = 0; r < blits; ++r) {
            mosaico_mtx2_blit(target, tile, &texture, 0, rows[r] % (side - tile), tile, tile,
                              light);
            sink = (uint16_t)(sink + target[0]);
        }
        double mtx2_ns = (now_seconds() - t0) * 1e9 / pixels;

        printf("  | %-8s raw %5.2f  mtx2 %5.2f ns/px (%.2fx)",
               shaded ? "shaded" : "unshaded", raw_ns, mtx2_ns, mtx2_ns / raw_ns);
    }
    printf("\n");

    /* The blit must agree with the span path it is meant to replace. */
    uint16_t *reference = malloc(sizeof(uint16_t) * (size_t)tile * (size_t)tile);
    for (int y = 0; y < tile; ++y) {
        mosaico_mtx2_span_constv(reference + (size_t)y * (size_t)tile, &texture, 5 << 16,
                                 1 << 16, 7 + y, tile, 192U);
    }
    mosaico_mtx2_blit(target, tile, &texture, 5, 7, tile, tile, 192U);
    if (memcmp(reference, target, sizeof(uint16_t) * (size_t)tile * (size_t)tile) != 0) {
        fprintf(stderr, "mosaico_mtx2_blit disagrees with the span path\n");
        exit(1);
    }

    free(reference);
    free(target);
    free(rows);
    free(raw);
    free(blob);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s DIR ORACLE_BLOCKS\n", argv[0]);
        return 2;
    }
    mosaico_shade_lut_init();
    if (check_oracle(argv[1], "punch", atoi(argv[2])) != 0
        || check_oracle(argv[1], "opaque", atoi(argv[2])) != 0) {
        return 1;
    }
    printf("MTX2 sampler benchmark, 1:1 sampling, scattered rows\n");
    printf("(Host CPU, informational. Host has no flash cache, so this measures\n");
    printf(" ALU cost and DRAM pressure only; it does not establish device FPS.)\n");
    benchmark("L1-resident", 64);    /* raw 8 KB   */
    benchmark("L2-resident", 512);   /* raw 512 KB */
    benchmark("DRAM-bound", 4096);   /* raw 32 MB  */
    return 0;
}
