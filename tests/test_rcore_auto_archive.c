// SPDX-License-Identifier: Apache-2.0
/* Automatic asset bridge linkage test: this caller NEVER references
 * raylib_lite_rcore_assets_* directly and links only against the static archive. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_assets_backend.h"
#include "frame_host.h"

raylib_lite_result_t raylib_lite_asset_mmap_mount(const raylib_lite_asset_store_config_t *config)
{ (void)config; return RAYLIB_LITE_NOT_SUPPORTED; }
void raylib_lite_asset_mmap_unmount(void) {}
bool raylib_lite_asset_mmap_is_mounted(void) { return false; }
raylib_lite_result_t raylib_lite_asset_mmap_open(const char *name, raylib_lite_asset_view_t *view)
{ (void)name; (void)view; return RAYLIB_LITE_NOT_FOUND; }
raylib_lite_result_t raylib_lite_asset_mmap_open_id(raylib_lite_asset_id_t id, raylib_lite_asset_view_t *view)
{ (void)id; (void)view; return RAYLIB_LITE_NOT_FOUND; }

static const char descriptor[] =
    "info face=\"registered\" size=8\n"
    "common lineHeight=8 base=8 scaleW=8 scaleH=4 pages=1\n"
    "page id=0 file=\"atlas.png\"\n"
    "chars count=1\n"
    "char id=65 x=0 y=0 width=4 height=4 xoffset=0 yoffset=0 xadvance=4 page=0\n";

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file = fopen(argv[1], "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long length = ftell(file);
    assert(length > 0 && !fseek(file, 0, SEEK_SET));
    unsigned char *png = malloc((size_t)length);
    assert(png && fread(png, 1, (size_t)length, file) == (size_t)length);
    fclose(file);

    frame_surface_t surface;
    assert(frame_surface_init(&surface, 32, 16) == 0);
    SetTraceLogLevel(LOG_NONE);

    /* 1. Invalid asset_root fails InitWindow and leaves IsWindowReady() false. */
    raylib_lite_rcore_config_t bad = {
        .video = frame_surface_backend(&surface),
        .clock = frame_host_clock(),
        .asset_root = "../escape",
    };
    assert(raylib_lite_rcore_configure(&bad) == RAYLIB_LITE_OK);
    InitWindow(32, 16, "bad-root");
    assert(!IsWindowReady() && WindowShouldClose());

    /* 2. Valid asset_root pulls raylib_lite_rcore_assets.o from the static .a archive! */
    assert(raylib_lite_asset_register_memory("bundle/ui.fnt", descriptor, sizeof(descriptor) - 1) == RAYLIB_LITE_OK);
    assert(raylib_lite_asset_register_memory("bundle/atlas.png", png, (size_t)length) == RAYLIB_LITE_OK);
    raylib_lite_rcore_config_t ok = {
        .video = frame_surface_backend(&surface),
        .clock = frame_host_clock(),
        .asset_root = "bundle",
    };
    assert(raylib_lite_rcore_configure(&ok) == RAYLIB_LITE_OK);
    InitWindow(32, 16, "auto-archive");
    assert(IsWindowReady());
    Font font = LoadFont("ui.fnt");
    assert(font.texture.id && font.texture.id != GetFontDefault().texture.id);
    assert(font.baseSize == 8 && font.glyphCount == 1);
    UnloadFont(font);
    CloseWindow();
    assert(!IsWindowReady());

    raylib_lite_assets_unmount();
    frame_surface_free(&surface);
    free(png);
    puts("rcore auto archive: ok");
    return 0;
}
