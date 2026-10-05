// SPDX-License-Identifier: Apache-2.0
#include "mosaico_game_assets.h"
#include "mosaico_game_assets_backend.h"

#include <stdlib.h>
#include <string.h>
#define EMBEDDED_ASSET_CAPACITY 32
static mosaico_asset_view_t s_embedded[EMBEDDED_ASSET_CAPACITY];
static size_t s_embedded_count;

/* Backing layer: one mount over either a resident image alias (zero-copy
 * views, the legacy mount_memory semantics) or bounded reads (per-asset
 * lazy materialization). Both forms share the TOC parsing and validation. */
typedef struct {
    mosaico_asset_id_t id;
    const char *name;        /* image alias or pointer into the TOC copy */
    const uint8_t *data;     /* payload alias (image form only) */
    uint8_t *mat_buf;        /* mount-owned materialized copy (read form) */
    uint32_t backing_offset; /* payload start in MMAP image coordinates */
    size_t size;
    size_t refs;             /* materialization reference count */
} backing_entry_t;

static struct {
    bool active;
    const uint8_t *image;               /* NULL in read form */
    void *ctx;                          /* read callback context */
    mosaico_asset_backing_read_fn read; /* NULL in image form */
    uint32_t image_size;
    backing_entry_t entries[EMBEDDED_ASSET_CAPACITY];
    size_t count;
    uint8_t *toc_copy;      /* mount-owned TOC bytes (read form) */
    size_t toc_bytes;
    size_t materialized_bytes;
    size_t materialized_peak;
} s_backing;

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

const mosaico_asset_t *mosaico_game_asset_find(const mosaico_asset_t *assets,size_t count,mosaico_asset_id_t id){
    if(!assets)return NULL;
    for(size_t i=0;i<count;++i)if(assets[i].id==id)return &assets[i];
    return NULL;
}

mosaico_asset_id_t mosaico_game_asset_id(const char *name)
{
    uint32_t hash = 2166136261U;
    if (!name) return 0;
    while (*name) hash = (hash ^ (uint8_t)*name++) * 16777619U;
    return hash;
}

esp_err_t mosaico_game_assets_mount_backing(
        const mosaico_asset_backing_t *backing, int max_files,
        uint16_t checksum)
{
    if (!backing || max_files <= 0 || mosaico_asset_mmap_is_mounted() ||
            s_backing.active)
        return ESP_ERR_INVALID_ARG;
    if (backing->size < 32) return ESP_ERR_INVALID_ARG;
    const uint8_t *image = backing->image;
    uint8_t header[32];
    if (image) {
        memcpy(header, image, sizeof(header));
    } else {
        if (!backing->read) return ESP_ERR_INVALID_ARG;
        esp_err_t err = backing->read(backing->ctx, 0, header, sizeof(header));
        if (err != ESP_OK) return err;
    }
    if (memcmp(header, "MMAP", 4) != 0) return ESP_ERR_INVALID_ARG;
    uint32_t name_len = read_u32(header + 8);
    uint32_t files = read_u32(header + 12);
    uint32_t stored_checksum = read_u32(header + 16);
    uint32_t payload_len = read_u32(header + 20);
    if (files == 0) return ESP_OK;
    if ((int)files > max_files || files > EMBEDDED_ASSET_CAPACITY ||
        name_len == 0 || name_len > 256)
        return ESP_ERR_INVALID_SIZE;
    size_t table_bytes = (size_t)files * (name_len + 12U);
    if ((size_t)32 + payload_len > backing->size || table_bytes > payload_len)
        return ESP_ERR_INVALID_SIZE;
    if (checksum && (uint16_t)stored_checksum != checksum)
        return ESP_ERR_INVALID_CRC;
    uint8_t *toc_copy = NULL;
    const uint8_t *table;
    if (image) {
        table = image + 32;
    } else {
        toc_copy = malloc(table_bytes);
        if (!toc_copy) return ESP_ERR_NO_MEM;
        esp_err_t err = backing->read(backing->ctx, 32, toc_copy, table_bytes);
        if (err != ESP_OK) { free(toc_copy); return err; }
        table = toc_copy;
    }
    const uint8_t *data_base = table + table_bytes;
    size_t data_len = payload_len - table_bytes;
    for (uint32_t i = 0; i < files; ++i) {
        const uint8_t *entry = table + (size_t)i * (name_len + 12U);
        if (entry[0] == 0 || !memchr(entry, 0, name_len)) {
            free(toc_copy);
            return ESP_ERR_INVALID_ARG;
        }
        uint32_t length = read_u32(entry + name_len);
        uint32_t offset = read_u32(entry + name_len + 4);
        if ((size_t)offset + 2U + length > data_len) {
            free(toc_copy);
            return ESP_ERR_INVALID_SIZE;
        }
        s_backing.entries[i] = (backing_entry_t){
            .id = mosaico_game_asset_id((const char *)entry),
            .name = (const char *)entry,
            .data = image ? data_base + offset + 2 : NULL,
            .backing_offset = 32U + (uint32_t)table_bytes + offset + 2U,
            .size = length,
        };
    }
    s_backing.image = image;
    s_backing.ctx = backing->ctx;
    s_backing.read = backing->read;
    s_backing.image_size = backing->size;
    s_backing.toc_copy = toc_copy;
    s_backing.toc_bytes = toc_copy ? table_bytes : 0;
    s_backing.count = files;
    s_backing.active = true;
    return ESP_OK;
}

esp_err_t mosaico_game_assets_mount(const mosaico_asset_store_config_t *config)
{
    if (!config || config->max_files <= 0 ||
            mosaico_asset_mmap_is_mounted() || s_backing.active)
        return ESP_ERR_INVALID_ARG;
    if (config->image) {
        if (config->image_size > UINT32_MAX) return ESP_ERR_INVALID_SIZE;
        mosaico_asset_backing_t backing = {
            .size = (uint32_t)config->image_size,
            .ctx = NULL,
            .read = NULL,
            .image = config->image,
        };
        return mosaico_game_assets_mount_backing(&backing, config->max_files,
                                                 config->checksum);
    }
    return mosaico_asset_mmap_mount(config);
}

esp_err_t mosaico_game_asset_register_memory(const char *name,
                                              const void *data, size_t size)
{
    if (!name || !data || !size) return ESP_ERR_INVALID_ARG;
    mosaico_asset_id_t id=mosaico_game_asset_id(name);
    for(size_t i=0;i<s_embedded_count;++i)
        if(s_embedded[i].id==id)return ESP_ERR_INVALID_STATE;
    if(s_embedded_count>=EMBEDDED_ASSET_CAPACITY)return ESP_ERR_NO_MEM;
    s_embedded[s_embedded_count++]=(mosaico_asset_view_t){
        .id=id,.name=name,.data=data,.size=size};
    return ESP_OK;
}

void mosaico_game_assets_unmount(void)
{
    mosaico_asset_mmap_unmount();
    for (size_t i = 0; i < s_backing.count; ++i)
        free(s_backing.entries[i].mat_buf);
    free(s_backing.toc_copy);
    size_t peak = s_backing.materialized_peak;
    memset(&s_backing, 0, sizeof(s_backing));
    s_backing.materialized_peak = peak;
}

bool mosaico_game_assets_is_mounted(void)
{
    return mosaico_asset_mmap_is_mounted() || s_backing.active;
}

size_t mosaico_game_assets_toc_bytes(void) { return s_backing.toc_bytes; }
size_t mosaico_game_assets_materialized_bytes(void)
{
    return s_backing.materialized_bytes;
}
size_t mosaico_game_assets_materialized_peak(void)
{
    return s_backing.materialized_peak;
}

static esp_err_t backing_open_entry(size_t index, mosaico_asset_view_t *out)
{
    backing_entry_t *entry = &s_backing.entries[index];
    if (!entry->mat_buf && !s_backing.image) {
        uint8_t *buf = malloc(entry->size ? entry->size : 1);
        if (!buf) return ESP_ERR_NO_MEM;
        esp_err_t err = s_backing.read(s_backing.ctx, entry->backing_offset,
                                       buf, entry->size);
        if (err != ESP_OK) { free(buf); return err; }
        entry->mat_buf = buf;
        entry->refs = 1;
        s_backing.materialized_bytes += entry->size;
        if (s_backing.materialized_bytes > s_backing.materialized_peak)
            s_backing.materialized_peak = s_backing.materialized_bytes;
    } else if (entry->mat_buf) {
        entry->refs++;
    }
    *out = (mosaico_asset_view_t){
        .id = entry->id,
        .name = entry->name,
        .data = entry->mat_buf ? entry->mat_buf : entry->data,
        .size = entry->size,
    };
    return ESP_OK;
}

void mosaico_game_asset_release(mosaico_asset_view_t *view)
{
    if (!view || !view->data) return;
    for (size_t i = 0; i < s_backing.count; ++i) {
        backing_entry_t *entry = &s_backing.entries[i];
        if (entry->mat_buf != view->data) continue;
        if (entry->refs > 0 && --entry->refs == 0) {
            free(entry->mat_buf);
            entry->mat_buf = NULL;
            s_backing.materialized_bytes -= entry->size;
        }
        break;
    }
    view->data = NULL;
}

esp_err_t mosaico_game_asset_open(const char *name, mosaico_asset_view_t *out)
{
    if (!name || !out) return ESP_ERR_INVALID_ARG;
    if (mosaico_asset_mmap_is_mounted()) {
        esp_err_t error = mosaico_asset_mmap_open(name, out);
        if (error == ESP_OK) return ESP_OK;
        if (error != ESP_ERR_NOT_FOUND) return error;
    }
    for(size_t i=0;i<s_backing.count;++i)
        if(strcmp(s_backing.entries[i].name,name)==0)
            return backing_open_entry(i,out);
    for(size_t i=0;i<s_embedded_count;++i)
        if(strcmp(s_embedded[i].name,name)==0){*out=s_embedded[i];return ESP_OK;}
    return ESP_ERR_NOT_FOUND;
}

esp_err_t mosaico_game_asset_open_id(mosaico_asset_id_t id,
                                     mosaico_asset_view_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    if (mosaico_asset_mmap_is_mounted()) {
        esp_err_t error = mosaico_asset_mmap_open_id(id, out);
        if (error == ESP_OK) return ESP_OK;
        if (error != ESP_ERR_NOT_FOUND) return error;
    }
    for(size_t i=0;i<s_backing.count;++i)
        if(s_backing.entries[i].id==id)
            return backing_open_entry(i,out);
    for(size_t i=0;i<s_embedded_count;++i)
        if(s_embedded[i].id==id){*out=s_embedded[i];return ESP_OK;}
    return ESP_ERR_NOT_FOUND;
}

struct mosaico_asset_stream {
    size_t entry;          /* backing entry index; only for read streams */
    const uint8_t *alias;  /* non-NULL when the bytes are already resident */
    size_t size;
    size_t pos;
};

static esp_err_t stream_init(const char *name, mosaico_asset_stream_t *s)
{
    memset(s, 0, sizeof(*s));
    s->entry = (size_t)-1;
    if (mosaico_asset_mmap_is_mounted()) {
        mosaico_asset_view_t view = {0};
        esp_err_t error = mosaico_asset_mmap_open(name, &view);
        if (error == ESP_OK) {
            s->alias = view.data;
            s->size = view.size;
            return ESP_OK;
        }
        if (error != ESP_ERR_NOT_FOUND) return error;
    }
    for (size_t i = 0; i < s_backing.count; ++i) {
        if (strcmp(s_backing.entries[i].name, name) == 0) {
            s->size = s_backing.entries[i].size;
            if (s_backing.image) s->alias = s_backing.entries[i].data;
            else s->entry = i;
            return ESP_OK;
        }
    }
    for (size_t i = 0; i < s_embedded_count; ++i) {
        if (strcmp(s_embedded[i].name, name) == 0) {
            s->alias = s_embedded[i].data;
            s->size = s_embedded[i].size;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t mosaico_game_asset_stream_open(const char *name, uint32_t skip,
                                         mosaico_asset_stream_t **out)
{
    if (!name || !out) return ESP_ERR_INVALID_ARG;
    mosaico_asset_stream_t s;
    esp_err_t err = stream_init(name, &s);
    if (err != ESP_OK) return err;
    if (skip > s.size) return ESP_ERR_INVALID_ARG;
    s.pos = skip;
    mosaico_asset_stream_t *stream = malloc(sizeof(*stream));
    if (!stream) return ESP_ERR_NO_MEM;
    *stream = s;
    *out = stream;
    return ESP_OK;
}

esp_err_t mosaico_game_asset_stream_read(mosaico_asset_stream_t *s,
                                         void *dst, size_t len,
                                         size_t *read_out)
{
    if (!s || (!dst && len) || !read_out) return ESP_ERR_INVALID_ARG;
    size_t avail = s->size - s->pos;
    size_t take = len < avail ? len : avail;
    if (take) {
        if (s->alias) {
            memcpy(dst, s->alias + s->pos, take);
        } else {
            const backing_entry_t *entry = &s_backing.entries[s->entry];
            esp_err_t err = s_backing.read(s_backing.ctx,
                    entry->backing_offset + (uint32_t)s->pos, dst, take);
            if (err != ESP_OK) { *read_out = 0; return err; }
        }
        s->pos += take;
    }
    *read_out = take;
    return ESP_OK;
}

void mosaico_game_asset_stream_close(mosaico_asset_stream_t *s)
{
    free(s);
}
