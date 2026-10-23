// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "raylib_lite_rcore.h"
#include "raylib_lite_rcore_assets.h"
#include "raylib_lite_assets.h"
#include "raylib_lite_assets_backend.h"
#include "frame_host.h"

/* Registered-memory assets only: no backend filesystem can mask a bad path. */
raylib_lite_result_t raylib_lite_asset_mmap_mount(const raylib_lite_asset_store_config_t *config)
{ (void)config; return RAYLIB_LITE_NOT_SUPPORTED; }
void raylib_lite_asset_mmap_unmount(void) {}
bool raylib_lite_asset_mmap_is_mounted(void) { return false; }
raylib_lite_result_t raylib_lite_asset_mmap_open(const char *name, raylib_lite_asset_view_t *view)
{ (void)name; (void)view; return RAYLIB_LITE_NOT_FOUND; }
raylib_lite_result_t raylib_lite_asset_mmap_open_id(raylib_lite_asset_id_t id, raylib_lite_asset_view_t *view)
{ (void)id; (void)view; return RAYLIB_LITE_NOT_FOUND; }

static const unsigned char payload[]={1,2,3,0,255};
static const char descriptor[]=
    "info face=\"registered\" size=8\n"
    "common lineHeight=8 base=8 scaleW=8 scaleH=4 pages=1\n"
    "page id=0 file=\"images/../atlas.png\"\n"
    "chars count=2\n"
    "char id=65 x=0 y=0 width=4 height=4 xoffset=0 yoffset=0 xadvance=4 page=0\n"
    "char id=20013 x=4 y=0 width=4 height=4 xoffset=0 yoffset=0 xadvance=4 page=0\n";

static void check_native_font(frame_surface_t *surface)
{
    Font font=LoadFont("./fonts//ui.fnt");
    assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_OK);
    /* Verify the requested resource really loaded, not LoadFont's default. */
    assert(font.texture.id && font.texture.id != GetFontDefault().texture.id);
    assert(font.baseSize == 8 && font.glyphCount == 2);
    assert(font.glyphs[0].value == 65 && font.glyphs[1].value == 20013);
    Color red=GetImageColor(font.glyphs[0].image,1,1);
    Color green=GetImageColor(font.glyphs[1].image,1,1);
    assert(red.r == 255 && !red.g && green.g == 255 && !green.r);
    const char text[]="A\xe4\xb8\xad";
    assert(MeasureTextEx(font,text,8,0).x == 8);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTextEx(font,text,(Vector2){1,1},8,0,WHITE);
    EndDrawing();
    assert(surface->pixels[2*surface->stride+2] == 0xf800);
    assert(surface->pixels[2*surface->stride+6] == 0x07e0);
    UnloadFont(font);
    Font missing=LoadFont("fonts/missing.fnt");
    assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_NOT_FOUND);
    assert(!missing.texture.id);
    UnloadFont(missing);
    missing=LoadFont("fonts/missing.png");
    assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_NOT_FOUND);
    assert(missing.texture.id == GetFontDefault().texture.id);
    UnloadFont(missing);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    FILE *file=fopen(argv[1],"rb");
    assert(file && !fseek(file,0,SEEK_END));
    long length=ftell(file);
    assert(length > 0 && !fseek(file,0,SEEK_SET));
    unsigned char *png=malloc((size_t)length);
    assert(png && fread(png,1,(size_t)length,file) == (size_t)length);
    fclose(file);
    frame_surface_t surface;
    assert(frame_surface_init(&surface,32,16) == 0);
    raylib_lite_rcore_config_t config={.video=frame_surface_backend(&surface),.clock=frame_host_clock()};
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    SetTraceLogLevel(LOG_NONE);
    InitWindow(32,16,"native-assets-test");
    assert(IsWindowReady());
    SetTargetFPS(0);
    assert(raylib_lite_rcore_assets_attach(NULL) == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_rcore_assets_attach("/absolute") == RAYLIB_LITE_INVALID_ARGUMENT);
    assert(raylib_lite_rcore_assets_attach("../escape") == RAYLIB_LITE_INVALID_ARGUMENT);
    for (unsigned iteration=0; iteration<8; ++iteration) {
        assert(raylib_lite_asset_register_memory("bundle/fonts/ui.fnt",descriptor,sizeof(descriptor)-1) == RAYLIB_LITE_OK);
        assert(raylib_lite_asset_register_memory("bundle/fonts/atlas.png",png,(size_t)length) == RAYLIB_LITE_OK);
        assert(raylib_lite_asset_register_memory("bundle/data.bin",payload,sizeof(payload)) == RAYLIB_LITE_OK);
        assert(raylib_lite_rcore_assets_attach("./bundle//.") == RAYLIB_LITE_OK);
        assert(raylib_lite_rcore_assets_attach("other") == RAYLIB_LITE_BUSY);
        int bytes=-1;
        unsigned char *copy=LoadFileData("./nested/../data.bin",&bytes);
        assert(copy && bytes == sizeof(payload) && copy != payload);
        assert(!memcmp(copy,payload,sizeof(payload)));
        copy[0]=42;
        assert(payload[0] == 1);
        char *text=LoadFileText("fonts/ui.fnt");
        assert(text && text != descriptor && strlen(text) == sizeof(descriptor)-1);
        assert(!strcmp(text,descriptor));
        const char *invalid[]={"/data.bin","../bundle/data.bin","x/../../data.bin",
                               "C:/data.bin","asset://data.bin","fonts\\ui.fnt","."};
        for (size_t i=0; i<sizeof(invalid)/sizeof(*invalid); ++i) {
            bytes=123;
            assert(!LoadFileData(invalid[i],&bytes) && bytes == 0);
            assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_INVALID_ARGUMENT);
        }
        char long_path[300]; memset(long_path,'x',sizeof(long_path)); long_path[299]=0;
        assert(!LoadFileText(long_path));
        assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_INVALID_SIZE);
        assert(!LoadFileText("missing.txt"));
        assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_NOT_FOUND);
        check_native_font(&surface);
        raylib_lite_rcore_assets_detach();
        raylib_lite_rcore_assets_detach();
        assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_NOT_READY);
        raylib_lite_assets_unmount();
        assert(copy[0] == 42 && !strcmp(text,descriptor));
        UnloadFileData(copy);
        UnloadFileText(text);
        /* After detach, upstream's normal filesystem reader is restored. */
        unsigned char *disk=LoadFileData(argv[1],&bytes);
        assert(disk && bytes == length && !memcmp(disk,png,(size_t)length));
        UnloadFileData(disk);
    }
    CloseWindow();
    /* Invalid asset_root must fail InitWindow cleanly: IsWindowReady() stays false
     * and the platform can be reconfigured immediately. */
    config.asset_root="../escape";
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    InitWindow(32,16,"native-assets-bad-root");
    assert(!IsWindowReady() && WindowShouldClose());
    assert(raylib_lite_rcore_last_acquire_result() == RAYLIB_LITE_NOT_READY);
    /* Automatic platform attachment via config.asset_root: no manual attach call. */
    assert(raylib_lite_asset_register_memory("bundle/fonts/ui.fnt",descriptor,sizeof(descriptor)-1) == RAYLIB_LITE_OK);
    assert(raylib_lite_asset_register_memory("bundle/fonts/atlas.png",png,(size_t)length) == RAYLIB_LITE_OK);
    config.asset_root="bundle";
    assert(raylib_lite_rcore_configure(&config) == RAYLIB_LITE_OK);
    InitWindow(32,16,"native-assets-auto");
    assert(IsWindowReady());
    check_native_font(&surface);
    CloseWindow();
    assert(!IsWindowReady());
    assert(raylib_lite_rcore_assets_last_result() == RAYLIB_LITE_NOT_READY);
    raylib_lite_assets_unmount();
    frame_surface_free(&surface);
    free(png);
    puts("native asset bridge: ok");
    return 0;
}
