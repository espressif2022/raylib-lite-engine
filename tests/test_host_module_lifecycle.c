// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "raylib_lite_host_game.h"
#include "raylib_lite_game_module.h"
#include "raylib_lite_raylib_impl.h"
#include "raylib_lite_host_video.h"
#include "raylib_lite_assets.h"
static int fail_init, shutdowns, video_stops, asset_stops;
static int initialize(void *state, const char *root) {
    assert(root); *(int *)state=42; return fail_init ? -1 : 0;
}
static void shutdown(void *state) { assert(*(int *)state==42); ++shutdowns; }
static void update(void *state) { (void)state; }
static int render(void *state) { (void)state; return 0; }
static const raylib_lite_game_module_v1_t module={
    .descriptor={.abi_version=1,.width=480,.height=480},
    .state_size=sizeof(int), .initialize=initialize,.shutdown=shutdown,.update=update,.render=render
};
const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void) { return &module; }
void raylib_lite_host_video_shutdown(void) { ++video_stops; }
void raylib_lite_assets_unmount(void) { ++asset_stops; }
void raylib_lite_raylib_init_window(int w,int h,const char *title) { (void)w;(void)h;(void)title; }
void raylib_lite_raylib_inject_action(int code,bool pressed) {(void)code;(void)pressed;}
void raylib_lite_raylib_inject_pointer(int track,int x,int y,bool pressed) {(void)track;(void)x;(void)y;(void)pressed;}
void raylib_lite_raylib_inject_imu(float x,float y,float z) {(void)x;(void)y;(void)z;}
raylib_lite_result_t raylib_lite_host_video_set_target(uint16_t *pixels,size_t stride,uint16_t w,uint16_t h) {
    (void)pixels;(void)stride;(void)w;(void)h;return RAYLIB_LITE_OK;
}
void raylib_lite_host_video_clear_target(void) {}
int main(void) {
    for(int i=0;i<3;++i) {
        fail_init=1;
        assert(raylib_lite_host_game_create_v1("test")==NULL);
        assert(shutdowns==2*i+1 && video_stops==shutdowns && asset_stops==shutdowns);
        fail_init=0;
        void *instance=raylib_lite_host_game_create_v1("test");assert(instance);
        raylib_lite_host_game_destroy_v1(instance);
        assert(shutdowns==2*i+2 && video_stops==shutdowns && asset_stops==shutdowns);
    }
    raylib_lite_host_game_destroy_v1(NULL);
    puts("host lifecycle: ok");
}
