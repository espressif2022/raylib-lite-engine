// SPDX-License-Identifier: Apache-2.0
#ifndef ESP_PLATFORM
#define _POSIX_C_SOURCE 200809L
#endif
#include "frame_host.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#else
#include <malloc.h>
#include <time.h>
#endif
#include "frame_scene_info.h"

int frame_compare_color_psram = -1;

static uint64_t now_ns(void)
{
#ifdef ESP_PLATFORM
    return (uint64_t)esp_timer_get_time() * 1000ull;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
#endif
}

static uint64_t mono_us(void *context)
{
    (void)context;
    return now_ns() / 1000ull;
}

static void sleep_noop(void *context, uint64_t duration_us)
{
    (void)context;
    (void)duration_us;
}

raylib_lite_clock_t frame_host_clock(void)
{
    return (raylib_lite_clock_t){ .monotonic_us = mono_us, .sleep_for_us = sleep_noop };
}

#ifndef ESP_PLATFORM
static long long heap_bytes(void)
{
    struct mallinfo2 info = mallinfo2();
    /* mmap-backed blocks are reported separately from the main arena. */
    return (long long)info.uordblks + (long long)info.hblkhd;
}
#endif

int frame_surface_init(frame_surface_t *surface, int width, int height)
{
    memset(surface, 0, sizeof(*surface));
    if (width < 1 || height < 1 || width > 1024 || height > 1024) return -1;
#ifdef ESP_PLATFORM
    size_t bytes = (size_t)width * (size_t)height * sizeof(uint16_t);
    surface->pixels = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (surface->pixels) memset(surface->pixels, 0, bytes);
#else
    surface->pixels = calloc((size_t)width * (size_t)height, sizeof(uint16_t));
#endif
    if (!surface->pixels) return -1;
    surface->width = width;
    surface->height = height;
    surface->stride = (size_t)width;
    return 0;
}

void frame_surface_free(frame_surface_t *surface)
{
#ifdef ESP_PLATFORM
    heap_caps_free(surface->pixels);
#else
    free(surface->pixels);
#endif
    memset(surface, 0, sizeof(*surface));
}

static raylib_lite_result_t surface_info(void *context, raylib_lite_video_info_t *out)
{
    frame_surface_t *surface = context;
    *out = (raylib_lite_video_info_t){
        (uint16_t)surface->width, (uint16_t)surface->height, surface->stride,
        RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t surface_acquire(void *context, raylib_lite_frame_t *out)
{
    frame_surface_t *surface = context;
    if (surface->acquired) return RAYLIB_LITE_INVALID_STATE;
    surface->acquired = 1;
    surface->acquire_ns = now_ns();
    surface->last_copy_ns = 0;
    *out = (raylib_lite_frame_t){
        surface->pixels, (uint16_t)surface->width, (uint16_t)surface->height,
        surface->stride, 1,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t surface_present(void *context, raylib_lite_frame_t *frame)
{
    frame_surface_t *surface = context;
    if (!surface->acquired || !frame || frame->token != 1) return RAYLIB_LITE_INVALID_STATE;
    surface->last_copy_ns = now_ns() - surface->acquire_ns;
    surface->acquired = 0;
    return RAYLIB_LITE_OK;
}

static void surface_discard(void *context, raylib_lite_frame_t *frame)
{
    frame_surface_t *surface = context;
    (void)frame;
    surface->acquired = 0;
    surface->last_copy_ns = 0;
}

static raylib_lite_result_t surface_flush(void *context, uint32_t timeout_ms)
{
    (void)context;
    (void)timeout_ms;
    return RAYLIB_LITE_OK;
}

raylib_lite_video_backend_t frame_surface_backend(frame_surface_t *surface)
{
    return (raylib_lite_video_backend_t){
        .context = surface,
        .get_info = surface_info,
        .acquire = surface_acquire,
        .present = surface_present,
        .discard = surface_discard,
        .flush = surface_flush,
    };
}

static uint32_t hash_frame(const uint16_t *pixels, int count)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < count; ++i) hash = (hash ^ pixels[i]) * 16777619u;
    return hash;
}

static int write_frame(const char *path, const uint16_t *pixels, int count)
{
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    size_t wrote = fwrite(pixels, sizeof(uint16_t), (size_t)count, file);
    if (fclose(file) != 0 || wrote != (size_t)count) return -1;
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr, "usage: %s --width N --height N --output DIR [--samples N]\n", argv0);
}

int frame_host_run(int width, int height, int samples, const char *output,
                   const frame_ops_t *ops)
{
    if (!ops || width < 1 || height < 1 || samples < 1 || samples > 9) return 2;
    if (!output && samples < 1) return 2;
#ifdef ESP_PLATFORM
    size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#else
    if (!output) return 2;
    long long heap_before = heap_bytes();
#endif
    frame_compare_color_psram = -1;
    frame_surface_t surface;
    if (frame_surface_init(&surface, width, height) != 0) {
        fprintf(stderr, "frame buffer allocation failed\n");
        return 1;
    }
    void *user = NULL;
    long long color_bytes = 0, depth_bytes = 0;
    int setup = ops->setup(&surface, width, height, &user, &color_bytes, &depth_bytes);
#ifdef ESP_PLATFORM
    long long psram_used = (long long)psram_before - (long long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    long long internal_used = (long long)internal_before -
        (long long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int frame_psram = esp_ptr_external_ram(surface.pixels) ? 1 : 0;
    long long heap_delta = psram_used + internal_used;
#else
    long long heap_after = heap_bytes();
    long long heap_delta = heap_after - heap_before;
#endif
    if (setup != 0) {
        fprintf(stderr, "%s setup failed\n", ops->path);
        if (user) ops->shutdown(user);
        frame_surface_free(&surface);
        return 1;
    }

    int count = frame_scene_count();
    int pixels = width * height;
    for (int scene = 0; scene < count; ++scene) {
        ops->draw(user, scene, width, height);
        double frame_us[9], copy_us[9];
        for (int sample = 0; sample < samples; ++sample) {
            memset(surface.pixels, 0xA5, (size_t)pixels * sizeof(uint16_t));
            uint64_t started = now_ns();
            ops->draw(user, scene, width, height);
            frame_us[sample] = (double)(now_ns() - started) / 1000.0;
            copy_us[sample] = (double)ops->copy_ns(user) / 1000.0;
        }
        if (output) {
            char raw_path[512];
            snprintf(raw_path, sizeof(raw_path), "%s/%s-%s.raw", output, ops->path,
                     frame_scene_name(scene));
            if (write_frame(raw_path, surface.pixels, pixels) != 0) {
                fprintf(stderr, "failed to write %s\n", raw_path);
                ops->shutdown(user);
                frame_surface_free(&surface);
                return 1;
            }
        }
        printf("FRAMECOMPARE {\"path\":\"%s\",\"scene\":\"%s\",\"width\":%d,\"height\":%d,"
               "\"hash\":%" PRIu32 ",\"heap_bytes\":%lld,\"color_bytes\":%lld,\"depth_bytes\":%lld,"
               "\"frame_us\":[",
               ops->path, frame_scene_name(scene), width, height,
               hash_frame(surface.pixels, pixels), heap_delta,
               color_bytes, depth_bytes);
        for (int sample = 0; sample < samples; ++sample)
            printf("%s%.3f", sample ? "," : "", frame_us[sample]);
        printf("],\"copy_us\":[");
        for (int sample = 0; sample < samples; ++sample)
            printf("%s%.3f", sample ? "," : "", copy_us[sample]);
        printf("]");
#ifdef ESP_PLATFORM
        printf(",\"frame_psram\":%d,\"color_psram\":%d,\"psram_used\":%lld,\"internal_used\":%lld",
               frame_psram, frame_compare_color_psram, psram_used, internal_used);
#endif
        printf("}\n");
        fflush(stdout);
    }
    ops->shutdown(user);
    frame_surface_free(&surface);
    return 0;
}

int frame_host_main(int argc, char **argv, const frame_ops_t *ops)
{
    int width = 480, height = 480, samples = 3;
    const char *output = NULL;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (!value && strcmp(arg, "--help") != 0) {
            usage(argv[0]);
            return 2;
        }
        if (strcmp(arg, "--width") == 0) width = atoi(value);
        else if (strcmp(arg, "--height") == 0) height = atoi(value);
        else if (strcmp(arg, "--samples") == 0) samples = atoi(value);
        else if (strcmp(arg, "--output") == 0) output = value;
        else if (strcmp(arg, "--help") == 0) { usage(argv[0]); return 0; }
        else { usage(argv[0]); return 2; }
        if (strcmp(arg, "--help") != 0) ++i;
    }
    if (!output || samples < 1 || samples > 9) {
        usage(argv[0]);
        return 2;
    }
    return frame_host_run(width, height, samples, output, ops);
}
