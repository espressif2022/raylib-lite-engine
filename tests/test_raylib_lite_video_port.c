// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "raylib_lite_raylib_port.h"

enum { WIDTH = 7, HEIGHT = 5, STRIDE = 9 };

typedef struct {
    uint16_t pixels[STRIDE * HEIGHT];
    uint16_t latest[WIDTH * HEIGHT];
    bool acquired;
    bool have_latest;
    unsigned presents;
    unsigned discards;
    unsigned flushes;
    raylib_lite_result_t acquire_result;
    raylib_lite_result_t present_result;
} fake_video_t;

static raylib_lite_result_t fake_get_info(
    void *context, raylib_lite_video_info_t *out)
{
    (void)context;
    *out = (raylib_lite_video_info_t) {
        .width = WIDTH, .height = HEIGHT, .stride_pixels = STRIDE,
        .format = RAYLIB_LITE_PIXEL_RGB565_NATIVE,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t fake_acquire(void *context, raylib_lite_frame_t *out)
{
    fake_video_t *fake = context;
    if (fake->acquire_result != RAYLIB_LITE_OK) return fake->acquire_result;
    assert(!fake->acquired);
    fake->acquired = true;
    *out = (raylib_lite_frame_t) {
        .pixels = fake->pixels, .width = WIDTH, .height = HEIGHT,
        .stride_pixels = STRIDE, .token = 0x1234,
    };
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t fake_present(void *context, raylib_lite_frame_t *frame)
{
    fake_video_t *fake = context;
    assert(fake->acquired && frame->token == 0x1234);
    fake->acquired = false;
    ++fake->presents;
    if (fake->present_result == RAYLIB_LITE_OK) {
        for (size_t y = 0; y < HEIGHT; ++y)
            memcpy(fake->latest + y * WIDTH, frame->pixels + y * STRIDE,
                   WIDTH * sizeof(uint16_t));
        fake->have_latest = true;
    }
    return fake->present_result;
}

static void fake_discard(void *context, raylib_lite_frame_t *frame)
{
    fake_video_t *fake = context;
    assert(fake->acquired && frame->token == 0x1234);
    fake->acquired = false;
    ++fake->discards;
}

static raylib_lite_result_t fake_flush(void *context, uint32_t timeout_ms)
{
    fake_video_t *fake = context;
    (void)timeout_ms;
    ++fake->flushes;
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t fake_copy_latest(
    void *context, uint16_t *out, size_t capacity)
{
    fake_video_t *fake = context;
    if (!fake->have_latest) return RAYLIB_LITE_NOT_READY;
    if (capacity < WIDTH * HEIGHT) return RAYLIB_LITE_INVALID_ARGUMENT;
    memcpy(out, fake->latest, sizeof(fake->latest));
    return RAYLIB_LITE_OK;
}

static raylib_lite_video_backend_t backend_for(fake_video_t *fake)
{
    return (raylib_lite_video_backend_t) {
        .context = fake, .get_info = fake_get_info, .acquire = fake_acquire,
        .present = fake_present, .discard = fake_discard, .flush = fake_flush,
        .copy_latest = fake_copy_latest,
    };
}

int main(void)
{
    fake_video_t fake = {0};
    raylib_lite_video_backend_t backend = backend_for(&fake);
    assert(raylib_lite_raylib_port_init_backend(&backend) == RAYLIB_LITE_OK);
    assert(raylib_lite_raylib_port_last_acquire_result() == RAYLIB_LITE_NOT_READY);
    assert(raylib_lite_raylib_port_last_present_result() == RAYLIB_LITE_NOT_READY);

    uint16_t width = 0, height = 0;
    raylib_lite_raylib_port_get_dimensions(&width, &height);
    assert(width == WIDTH && height == HEIGHT);

    uint16_t *pixels = NULL;
    size_t stride = 0;
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) == RAYLIB_LITE_OK);
    assert(raylib_lite_raylib_port_last_acquire_result() == RAYLIB_LITE_OK);
    assert(pixels == fake.pixels && stride == STRIDE);
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) ==
           RAYLIB_LITE_INVALID_STATE);
    fake.pixels[0] = 0x55aa;
    assert(raylib_lite_raylib_port_present_frame() == RAYLIB_LITE_OK);
    assert(raylib_lite_raylib_port_last_present_result() == RAYLIB_LITE_OK);
    assert(fake.presents == 1 && !fake.acquired);

    uint16_t copy[WIDTH * HEIGHT] = {0};
    assert(raylib_lite_raylib_port_copy_latest(copy, WIDTH * HEIGHT) ==
           RAYLIB_LITE_OK);
    assert(copy[0] == 0x55aa);

    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) == RAYLIB_LITE_OK);
    raylib_lite_raylib_port_discard_frame();
    assert(fake.discards == 1 && !fake.acquired);

    fake.acquire_result = RAYLIB_LITE_BUSY;
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) == RAYLIB_LITE_BUSY);
    assert(raylib_lite_raylib_port_last_acquire_result() == RAYLIB_LITE_BUSY);
    assert(raylib_lite_raylib_port_last_present_result() == RAYLIB_LITE_NOT_READY);
    assert(pixels == NULL && stride == 0);
    fake.acquire_result = RAYLIB_LITE_OK;

    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) == RAYLIB_LITE_OK);
    fake.present_result = RAYLIB_LITE_IO_ERROR;
    assert(raylib_lite_raylib_port_present_frame() == RAYLIB_LITE_IO_ERROR);
    assert(raylib_lite_raylib_port_last_present_result() == RAYLIB_LITE_IO_ERROR);
    assert(!fake.acquired);
    assert(raylib_lite_raylib_port_present_frame() == RAYLIB_LITE_INVALID_STATE);

    assert(raylib_lite_raylib_port_flush(0) == RAYLIB_LITE_OK);
    assert(fake.flushes == 1);
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) == RAYLIB_LITE_OK);
    raylib_lite_raylib_port_deinit();
    assert(fake.discards == 2 && !fake.acquired);
    assert(raylib_lite_raylib_port_begin_frame(&pixels, &stride) ==
           RAYLIB_LITE_INVALID_STATE);
    return 0;
}
