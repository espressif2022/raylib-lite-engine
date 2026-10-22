// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_rcore_assets.h"
#include "raylib_lite_assets.h"
#include "raylib.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>

#define PATH_CAPACITY 256U
static char s_root[PATH_CAPACITY];
static bool s_attached;
static raylib_lite_result_t s_last=RAYLIB_LITE_NOT_READY;

static raylib_lite_result_t normalize(const char *path, char *out)
{
    if (!path || path[0] == '/') return RAYLIB_LITE_INVALID_ARGUMENT;
    size_t length=0, used=0;
    while (path[length]) {
        unsigned char c=(unsigned char)path[length];
        if (c == '\\' || c == ':' || c < 32 || c == 127) return RAYLIB_LITE_INVALID_ARGUMENT;
        if (++length >= PATH_CAPACITY) return RAYLIB_LITE_INVALID_SIZE;
    }
    for (size_t begin=0; begin<length;) {
        size_t end=begin;
        while (end<length && path[end] != '/') ++end;
        size_t part=end-begin;
        if (!part || (part == 1 && path[begin] == '.')) { /* skip */ }
        else if (part == 2 && path[begin] == '.' && path[begin+1] == '.') {
            if (!used) return RAYLIB_LITE_INVALID_ARGUMENT;
            while (used && out[used-1] != '/') --used;
            if (used) --used;
        } else {
            if (used) out[used++]='/';
            memcpy(out+used,path+begin,part);
            used+=part;
        }
        begin=end+1;
    }
    out[used]='\0';
    return RAYLIB_LITE_OK;
}

static unsigned char *read_copy(const char *name, int *size, bool text)
{
    if (size) *size=0;
    if (!s_attached) { s_last=RAYLIB_LITE_NOT_READY; return NULL; }
    char relative[PATH_CAPACITY], full[PATH_CAPACITY];
    s_last=normalize(name,relative);
    if (s_last != RAYLIB_LITE_OK) return NULL;
    size_t root_length=strlen(s_root), length=strlen(relative);
    if (!length) { s_last=RAYLIB_LITE_INVALID_ARGUMENT; return NULL; }
    if (root_length+length+(root_length ? 1U : 0U) >= sizeof(full)) {
        s_last=RAYLIB_LITE_INVALID_SIZE; return NULL;
    }
    memcpy(full,s_root,root_length);
    if (root_length) full[root_length++]='/';
    memcpy(full+root_length,relative,length+1);
    raylib_lite_asset_view_t view={0};
    s_last=raylib_lite_asset_open(full,&view);
    if (s_last != RAYLIB_LITE_OK) return NULL;
    if (view.size > INT_MAX || view.size > UINT_MAX-1U || (!view.data && view.size)) {
        raylib_lite_asset_release(&view);
        s_last=RAYLIB_LITE_INVALID_SIZE; return NULL;
    }
    unsigned int bytes=(unsigned int)view.size;
    unsigned char *copy=MemAlloc(bytes+(text || !bytes ? 1U : 0U));
    if (!copy) {
        raylib_lite_asset_release(&view);
        s_last=RAYLIB_LITE_NO_MEMORY; return NULL;
    }
    if (bytes) memcpy(copy,view.data,bytes);
    if (text) copy[bytes]=0;
    if (size) *size=(int)bytes;
    raylib_lite_asset_release(&view);
    s_last=RAYLIB_LITE_OK;
    return copy;
}
static unsigned char *read_data(const char *name, int *size)
{
    return read_copy(name,size,false);
}
static char *read_text(const char *name)
{
    return (char *)read_copy(name,NULL,true);
}
raylib_lite_result_t raylib_lite_rcore_assets_attach(const char *logical_root)
{
    if (s_attached) return s_last=RAYLIB_LITE_BUSY;
    char normalized[PATH_CAPACITY];
    s_last=normalize(logical_root,normalized);
    if (s_last != RAYLIB_LITE_OK) return s_last;
    memcpy(s_root,normalized,strlen(normalized)+1);
    SetLoadFileDataCallback(read_data);
    SetLoadFileTextCallback(read_text);
    s_attached=true;
    return s_last=RAYLIB_LITE_OK;
}
void raylib_lite_rcore_assets_detach(void)
{
    if (s_attached) {
        SetLoadFileDataCallback(NULL);
        SetLoadFileTextCallback(NULL);
        s_attached=false;
        s_root[0]='\0';
    }
    s_last=RAYLIB_LITE_NOT_READY;
}
raylib_lite_result_t raylib_lite_rcore_assets_last_result(void) { return s_last; }
