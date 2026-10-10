// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib_lite_assets.h"
#include "raylib_lite_assets_backend.h"

#define NAME_LEN 16U
#define FILES 2U
#define ENTRY_SIZE (NAME_LEN + 12U)
#define TABLE_BYTES (FILES * ENTRY_SIZE)
#define CHECKSUM 0x1234U

typedef struct {
    uint8_t image[160];
    size_t size;
    int fail_read;
    size_t bytes_read;
} fake_backing_t;

static int s_mmap_mounted;
static const uint8_t s_partition_bytes[] = {'P','A','R','T'};

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void make_image(fake_backing_t *backing)
{
    memset(backing, 0, sizeof(*backing));
    uint8_t *image = backing->image;
    memcpy(image, "MMAP", 4);
    put_u32(image + 8, NAME_LEN);
    put_u32(image + 12, FILES);
    put_u32(image + 16, CHECKSUM);

    uint8_t *table = image + 32;
    memcpy(table, "alpha.bin", 10);
    put_u32(table + NAME_LEN, 5);
    put_u32(table + NAME_LEN + 4, 0);

    uint8_t *second = table + ENTRY_SIZE;
    memcpy(second, "beta.bin", 9);
    put_u32(second + NAME_LEN, 4);
    put_u32(second + NAME_LEN + 4, 7);

    uint8_t *data = table + TABLE_BYTES;
    data[0] = data[1] = 0;
    memcpy(data + 2, "ABCDE", 5);
    data[7] = data[8] = 0;
    memcpy(data + 9, "wxyz", 4);

    const uint32_t payload = TABLE_BYTES + 13U;
    put_u32(image + 20, payload);
    backing->size = 32U + payload;
}

static raylib_lite_result_t fake_read(void *context, uint32_t offset, void *dst, size_t len)
{
    fake_backing_t *backing = context;
    if (backing->fail_read) return RAYLIB_LITE_IO_ERROR;
    if ((size_t)offset > backing->size ||
            len > backing->size - (size_t)offset) {
        return RAYLIB_LITE_INVALID_SIZE;
    }
    memcpy(dst, backing->image + offset, len);
    backing->bytes_read += len;
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_asset_mmap_mount(const raylib_lite_asset_store_config_t *config)
{
    if (!config || !config->partition_label || s_mmap_mounted)
        return RAYLIB_LITE_INVALID_ARGUMENT;
    s_mmap_mounted = 1;
    return RAYLIB_LITE_OK;
}

void raylib_lite_asset_mmap_unmount(void)
{
    s_mmap_mounted = 0;
}

bool raylib_lite_asset_mmap_is_mounted(void)
{
    return s_mmap_mounted != 0;
}

raylib_lite_result_t raylib_lite_asset_mmap_open(const char *name, raylib_lite_asset_view_t *out)
{
    if (!s_mmap_mounted || !name || !out) return RAYLIB_LITE_NOT_FOUND;
    if (strcmp(name, "alpha.bin") != 0) return RAYLIB_LITE_NOT_FOUND;
    *out = (raylib_lite_asset_view_t) {
        .id = raylib_lite_asset_id(name),
        .name = "alpha.bin",
        .data = s_partition_bytes,
        .size = sizeof(s_partition_bytes),
    };
    return RAYLIB_LITE_OK;
}

raylib_lite_result_t raylib_lite_asset_mmap_open_id(raylib_lite_asset_id_t id,
                                     raylib_lite_asset_view_t *out)
{
    if (id != raylib_lite_asset_id("alpha.bin")) return RAYLIB_LITE_NOT_FOUND;
    return raylib_lite_asset_mmap_open("alpha.bin", out);
}

static void check_read_backing(fake_backing_t *source)
{
    const uint8_t embedded[] = {'E','M','B'};
    assert(raylib_lite_asset_register_memory(
        "alpha.bin", embedded, sizeof(embedded)) == RAYLIB_LITE_OK);

    raylib_lite_asset_backing_t backing = {
        .size = (uint32_t)source->size,
        .ctx = source,
        .read = fake_read,
    };
    assert(raylib_lite_assets_mount_backing(
        &backing, 32, CHECKSUM) == RAYLIB_LITE_OK);
    assert(raylib_lite_assets_is_mounted());
    assert(raylib_lite_assets_toc_bytes() == TABLE_BYTES);
    assert(raylib_lite_assets_materialized_bytes() == 0);

    raylib_lite_asset_view_t first = {0}, second = {0};
    assert(raylib_lite_asset_open("alpha.bin", &first) == RAYLIB_LITE_OK);
    assert(first.size == 5 && memcmp(first.data, "ABCDE", 5) == 0);
    assert(raylib_lite_assets_materialized_bytes() == 5);

    assert(raylib_lite_asset_open_id(
        raylib_lite_asset_id("alpha.bin"), &second) == RAYLIB_LITE_OK);
    assert(second.data == first.data);
    assert(raylib_lite_assets_materialized_bytes() == 5);

    raylib_lite_asset_release(&first);
    assert(first.data == NULL);
    assert(raylib_lite_assets_materialized_bytes() == 5);
    raylib_lite_asset_release(&second);
    assert(raylib_lite_assets_materialized_bytes() == 0);

    size_t live_before = raylib_lite_assets_materialized_bytes();
    raylib_lite_asset_stream_t *stream = NULL;
    assert(raylib_lite_asset_stream_open("beta.bin", 1, &stream) == RAYLIB_LITE_OK);
    char bytes[4] = {0};
    size_t got = 0;
    assert(raylib_lite_asset_stream_read(stream, bytes, 2, &got) == RAYLIB_LITE_OK);
    assert(got == 2 && memcmp(bytes, "xy", 2) == 0);
    assert(raylib_lite_assets_materialized_bytes() == live_before);
    raylib_lite_asset_stream_close(stream);

    size_t peak = raylib_lite_assets_materialized_peak();
    assert(peak >= 5);
    raylib_lite_assets_unmount();
    assert(!raylib_lite_assets_is_mounted());
    assert(raylib_lite_assets_toc_bytes() == 0);
    assert(raylib_lite_assets_materialized_bytes() == 0);
    assert(raylib_lite_assets_materialized_peak() == peak);
}

static void check_image_and_partition(fake_backing_t *source)
{
    raylib_lite_asset_store_config_t image = {
        .max_files = 32,
        .checksum = CHECKSUM,
        .image = source->image,
        .image_size = source->size,
    };
    assert(raylib_lite_assets_mount(&image) == RAYLIB_LITE_OK);
    raylib_lite_asset_view_t view = {0};
    assert(raylib_lite_asset_open("beta.bin", &view) == RAYLIB_LITE_OK);
    assert(view.size == 4 && memcmp(view.data, "wxyz", 4) == 0);
    assert(raylib_lite_assets_materialized_bytes() == 0);
    raylib_lite_asset_release(&view);
    assert(view.data == NULL);
    raylib_lite_assets_unmount();

    raylib_lite_asset_store_config_t partition = {
        .partition_label = "game_assets",
        .max_files = 32,
        .mmap_enable = true,
    };
    assert(raylib_lite_assets_mount(&partition) == RAYLIB_LITE_OK);
    assert(raylib_lite_asset_open("alpha.bin", &view) == RAYLIB_LITE_OK);
    assert(view.size == sizeof(s_partition_bytes));
    assert(memcmp(view.data, s_partition_bytes, sizeof(s_partition_bytes)) == 0);

    raylib_lite_asset_stream_t *stream = NULL;
    assert(raylib_lite_asset_stream_open("alpha.bin", 1, &stream) == RAYLIB_LITE_OK);
    uint8_t bytes[4] = {0};
    size_t got = 0;
    assert(raylib_lite_asset_stream_read(stream, bytes, 3, &got) == RAYLIB_LITE_OK);
    assert(got == 3 && memcmp(bytes, "ART", 3) == 0);
    raylib_lite_asset_stream_close(stream);
    raylib_lite_assets_unmount();
}

static void check_errors(fake_backing_t *source)
{
    raylib_lite_asset_backing_t backing = {
        .size = (uint32_t)source->size,
        .ctx = source,
        .read = fake_read,
    };
    assert(raylib_lite_assets_mount_backing(
        &backing, 32, CHECKSUM ^ 1U) == RAYLIB_LITE_INVALID_CRC);
    assert(raylib_lite_assets_mount_backing(
        &backing, 1, CHECKSUM) == RAYLIB_LITE_INVALID_SIZE);

    fake_backing_t malformed = *source;
    memset(malformed.image + 32, 'A', NAME_LEN);
    raylib_lite_asset_backing_t malformed_backing = {
        .size = (uint32_t)malformed.size,
        .ctx = &malformed,
        .read = fake_read,
    };
    assert(raylib_lite_assets_mount_backing(
        &malformed_backing, 32, CHECKSUM) == RAYLIB_LITE_INVALID_ARGUMENT);

    source->fail_read = 1;
    assert(raylib_lite_assets_mount_backing(
        &backing, 32, CHECKSUM) == RAYLIB_LITE_IO_ERROR);
    source->fail_read = 0;

    backing.size = 31;
    assert(raylib_lite_assets_mount_backing(
        &backing, 32, CHECKSUM) == RAYLIB_LITE_INVALID_ARGUMENT);
}

int main(void)
{
    fake_backing_t source;
    make_image(&source);
    check_read_backing(&source);
    check_image_and_partition(&source);
    check_errors(&source);
    puts("game assets: ok");
    return 0;
}
