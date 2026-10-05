// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "mosaico_game_assets.h"
#include "mosaico_game_assets_backend.h"

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

static esp_err_t fake_read(void *context, uint32_t offset, void *dst, size_t len)
{
    fake_backing_t *backing = context;
    if (backing->fail_read) return ESP_FAIL;
    if ((size_t)offset > backing->size ||
            len > backing->size - (size_t)offset) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(dst, backing->image + offset, len);
    backing->bytes_read += len;
    return ESP_OK;
}

esp_err_t mosaico_asset_mmap_mount(const mosaico_asset_store_config_t *config)
{
    if (!config || !config->partition_label || s_mmap_mounted)
        return ESP_ERR_INVALID_ARG;
    s_mmap_mounted = 1;
    return ESP_OK;
}

void mosaico_asset_mmap_unmount(void)
{
    s_mmap_mounted = 0;
}

bool mosaico_asset_mmap_is_mounted(void)
{
    return s_mmap_mounted != 0;
}

esp_err_t mosaico_asset_mmap_open(const char *name, mosaico_asset_view_t *out)
{
    if (!s_mmap_mounted || !name || !out) return ESP_ERR_NOT_FOUND;
    if (strcmp(name, "alpha.bin") != 0) return ESP_ERR_NOT_FOUND;
    *out = (mosaico_asset_view_t) {
        .id = mosaico_game_asset_id(name),
        .name = "alpha.bin",
        .data = s_partition_bytes,
        .size = sizeof(s_partition_bytes),
    };
    return ESP_OK;
}

esp_err_t mosaico_asset_mmap_open_id(mosaico_asset_id_t id,
                                     mosaico_asset_view_t *out)
{
    if (id != mosaico_game_asset_id("alpha.bin")) return ESP_ERR_NOT_FOUND;
    return mosaico_asset_mmap_open("alpha.bin", out);
}

static void check_read_backing(fake_backing_t *source)
{
    const uint8_t embedded[] = {'E','M','B'};
    assert(mosaico_game_asset_register_memory(
        "alpha.bin", embedded, sizeof(embedded)) == ESP_OK);

    mosaico_asset_backing_t backing = {
        .size = (uint32_t)source->size,
        .ctx = source,
        .read = fake_read,
    };
    assert(mosaico_game_assets_mount_backing(
        &backing, 32, CHECKSUM) == ESP_OK);
    assert(mosaico_game_assets_is_mounted());
    assert(mosaico_game_assets_toc_bytes() == TABLE_BYTES);
    assert(mosaico_game_assets_materialized_bytes() == 0);

    mosaico_asset_view_t first = {0}, second = {0};
    assert(mosaico_game_asset_open("alpha.bin", &first) == ESP_OK);
    assert(first.size == 5 && memcmp(first.data, "ABCDE", 5) == 0);
    assert(mosaico_game_assets_materialized_bytes() == 5);

    assert(mosaico_game_asset_open_id(
        mosaico_game_asset_id("alpha.bin"), &second) == ESP_OK);
    assert(second.data == first.data);
    assert(mosaico_game_assets_materialized_bytes() == 5);

    mosaico_game_asset_release(&first);
    assert(first.data == NULL);
    assert(mosaico_game_assets_materialized_bytes() == 5);
    mosaico_game_asset_release(&second);
    assert(mosaico_game_assets_materialized_bytes() == 0);

    size_t live_before = mosaico_game_assets_materialized_bytes();
    mosaico_asset_stream_t *stream = NULL;
    assert(mosaico_game_asset_stream_open("beta.bin", 1, &stream) == ESP_OK);
    char bytes[4] = {0};
    size_t got = 0;
    assert(mosaico_game_asset_stream_read(stream, bytes, 2, &got) == ESP_OK);
    assert(got == 2 && memcmp(bytes, "xy", 2) == 0);
    assert(mosaico_game_assets_materialized_bytes() == live_before);
    mosaico_game_asset_stream_close(stream);

    size_t peak = mosaico_game_assets_materialized_peak();
    assert(peak >= 5);
    mosaico_game_assets_unmount();
    assert(!mosaico_game_assets_is_mounted());
    assert(mosaico_game_assets_toc_bytes() == 0);
    assert(mosaico_game_assets_materialized_bytes() == 0);
    assert(mosaico_game_assets_materialized_peak() == peak);
}

static void check_image_and_partition(fake_backing_t *source)
{
    mosaico_asset_store_config_t image = {
        .max_files = 32,
        .checksum = CHECKSUM,
        .image = source->image,
        .image_size = source->size,
    };
    assert(mosaico_game_assets_mount(&image) == ESP_OK);
    mosaico_asset_view_t view = {0};
    assert(mosaico_game_asset_open("beta.bin", &view) == ESP_OK);
    assert(view.size == 4 && memcmp(view.data, "wxyz", 4) == 0);
    assert(mosaico_game_assets_materialized_bytes() == 0);
    mosaico_game_asset_release(&view);
    assert(view.data == NULL);
    mosaico_game_assets_unmount();

    mosaico_asset_store_config_t partition = {
        .partition_label = "game_assets",
        .max_files = 32,
        .mmap_enable = true,
    };
    assert(mosaico_game_assets_mount(&partition) == ESP_OK);
    assert(mosaico_game_asset_open("alpha.bin", &view) == ESP_OK);
    assert(view.size == sizeof(s_partition_bytes));
    assert(memcmp(view.data, s_partition_bytes, sizeof(s_partition_bytes)) == 0);

    mosaico_asset_stream_t *stream = NULL;
    assert(mosaico_game_asset_stream_open("alpha.bin", 1, &stream) == ESP_OK);
    uint8_t bytes[4] = {0};
    size_t got = 0;
    assert(mosaico_game_asset_stream_read(stream, bytes, 3, &got) == ESP_OK);
    assert(got == 3 && memcmp(bytes, "ART", 3) == 0);
    mosaico_game_asset_stream_close(stream);
    mosaico_game_assets_unmount();
}

static void check_errors(fake_backing_t *source)
{
    mosaico_asset_backing_t backing = {
        .size = (uint32_t)source->size,
        .ctx = source,
        .read = fake_read,
    };
    assert(mosaico_game_assets_mount_backing(
        &backing, 32, CHECKSUM ^ 1U) == ESP_ERR_INVALID_CRC);
    assert(mosaico_game_assets_mount_backing(
        &backing, 1, CHECKSUM) == ESP_ERR_INVALID_SIZE);

    fake_backing_t malformed = *source;
    memset(malformed.image + 32, 'A', NAME_LEN);
    mosaico_asset_backing_t malformed_backing = {
        .size = (uint32_t)malformed.size,
        .ctx = &malformed,
        .read = fake_read,
    };
    assert(mosaico_game_assets_mount_backing(
        &malformed_backing, 32, CHECKSUM) == ESP_ERR_INVALID_ARG);

    source->fail_read = 1;
    assert(mosaico_game_assets_mount_backing(
        &backing, 32, CHECKSUM) == ESP_FAIL);
    source->fail_read = 0;

    backing.size = 31;
    assert(mosaico_game_assets_mount_backing(
        &backing, 32, CHECKSUM) == ESP_ERR_INVALID_ARG);
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
