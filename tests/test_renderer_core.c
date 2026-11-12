// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "raylib_lite_renderer.h"
#include "raylib_lite_primitives_internal.h"

#define ATLAS_MAGIC 0x3141534dU
#define WALL_MAGIC 0x3157534dU

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, flags;
    uint32_t rgb_bytes, alpha_bytes;
    uint16_t pixels[4];
} texture_blob_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t width, height, frame_count, light_levels;
    uint32_t palette_entries, index_bytes;
    uint16_t light_lut[16 * 256];
    uint8_t indices[4];
} wall_blob_t;

static texture_blob_t s_texture = {
    .magic = ATLAS_MAGIC,
    .width = 2,
    .height = 2,
    .rgb_bytes = 8,
    .pixels = {1, 2, 3, 4},
};
static uint8_t s_invalid[4] = {0};
static wall_blob_t s_wall = {
    .magic = WALL_MAGIC,
    .width = 2,
    .height = 2,
    .light_levels = 16,
    .palette_entries = 256,
    .index_bytes = 4,
    .indices = {0, 1, 2, 3},
};
static unsigned s_open_count;
static unsigned s_release_count;
static const uint8_t *s_fixture;
static size_t s_fixture_size;

raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    if (!name || !out) return RAYLIB_LITE_INVALID_ARGUMENT;
    ++s_open_count;
    if (strcmp(name, "fixture") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .name = name, .data = s_fixture, .size = s_fixture_size,
        };
        return RAYLIB_LITE_OK;
    }
    if (strcmp(name, "texture") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .name = name, .data = (const uint8_t *)&s_texture,
            .size = sizeof(s_texture),
        };
        return RAYLIB_LITE_OK;
    }
    if (strcmp(name, "wall") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .name = name, .data = (const uint8_t *)&s_wall,
            .size = sizeof(s_wall),
        };
        return RAYLIB_LITE_OK;
    }
    if (strcmp(name, "invalid") == 0) {
        *out = (raylib_lite_asset_view_t) {
            .name = name, .data = s_invalid, .size = sizeof(s_invalid),
        };
        return RAYLIB_LITE_OK;
    }
    return RAYLIB_LITE_NOT_FOUND;
}

void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (!view || !view->data) return;
    ++s_release_count;
    view->data = NULL;
    view->size = 0;
}

static void reject_fixture(bool wall)
{
    unsigned opened = s_open_count, released = s_release_count;
    if (wall) assert(!raylib_lite_wall_atlas_load("fixture").descriptor);
    else assert(!raylib_lite_renderer_load_texture("fixture").id);
    assert(s_open_count == opened + 1 && s_release_count == released + 1);
}

static void packaged_texture_contract(void)
{
    /* Construct the file layout independently: 20-byte header, optional
     * 16-byte frame, RGB565 pixels, then one alpha byte per pixel. */
    uint8_t blob[48] = {0};
    texture_blob_t header = s_texture;
    header.alpha_bytes = 1;
    memcpy(blob, &header, 20);
    s_fixture = blob;
    s_fixture_size = 29;
    reject_fixture(false); /* Total size fits, but alpha plane is too short. */
    header.alpha_bytes = 5;
    memcpy(blob, &header, 20);
    s_fixture_size = 33;
    reject_fixture(false); /* Oversized alpha planes are malformed too. */

    header.frame_count = 1;
    header.alpha_bytes = 4;
    memcpy(blob, &header, 20);
    uint16_t frame[8] = {1, 0, 0, 0, 2, 2, 0, 0};
    memcpy(blob + 20, frame, 16);
    memcpy(blob + 36, s_texture.pixels, 8);
    memset(blob + 44, 255, 4);
    s_fixture_size = sizeof(blob);
    unsigned released = s_release_count;
    raylib_lite_renderer_texture_t valid = raylib_lite_renderer_load_texture("fixture");
    assert(valid.id && s_release_count == released);
    uint16_t target[4] = {0};
    raylib_lite_renderer_set_target(target, 2, 2, 2);
    raylib_lite_renderer_draw_texture_pro(valid,
        (raylib_lite_renderer_rect_t){0, 0, 2, 2},
        (raylib_lite_renderer_rect_t){0, 0, 2, 2},
        (raylib_lite_renderer_vec2_t){0, 0}, 0,
        (raylib_lite_renderer_color_t){255, 255, 255, 255});
    assert(!memcmp(target, s_texture.pixels, sizeof(target)));
    raylib_lite_renderer_unload_texture(valid);
    raylib_lite_renderer_unload_texture(valid);
    assert(s_release_count == released + 1);
    raylib_lite_renderer_set_target(NULL, 0, 0, 0);

    for (unsigned component = 2; component <= 5; ++component) {
        uint16_t saved = frame[component];
        frame[component] = component < 4 ? 2 : 0;
        memcpy(blob + 20, frame, 16);
        reject_fixture(false); /* Out-of-atlas position or empty extent. */
        frame[component] = saved;
    }
    memcpy(blob + 20, frame, 16);
    s_fixture_size = 47;
    reject_fixture(false); /* Truncated payload. */

    /* The same frame contract applies to wall atlases. */
    uint8_t wall_blob[sizeof(s_wall) + 16];
    memcpy(wall_blob, &s_wall, 20);
    uint16_t count = 1;
    memcpy(wall_blob + 8, &count, sizeof(count));
    memcpy(wall_blob + 20, frame, 16);
    memcpy(wall_blob + 36, (const uint8_t *)&s_wall + 20, sizeof(s_wall) - 20);
    s_fixture = wall_blob;
    s_fixture_size = sizeof(wall_blob);
    raylib_lite_wall_atlas_t wall = raylib_lite_wall_atlas_load("fixture");
    assert(wall.descriptor);
    raylib_lite_wall_atlas_unload(wall);
    frame[2] = 2;
    memcpy(wall_blob + 20, frame, 16);
    reject_fixture(true);
    assert(s_open_count == s_release_count);
}

static void primitive_surface_is_borrowed_and_neutral(void)
{
    uint16_t pixels[6] = {0, 0, 0x1234, 0, 0, 0x5678};
    raylib_lite_primitive_surface_t surface = {
        .pixels=pixels,.stride=3,.width=2,.height=2,
        .clip_x0=0,.clip_y0=0,.clip_x1=2,.clip_y1=2};
    raylib_lite_primitive_draw_rectangle(&surface,-1,-1,3,3,
        (raylib_lite_renderer_color_t){255,255,255,255});
    assert(pixels[0]==0xffff&&pixels[1]==0xffff&&pixels[3]==0xffff&&pixels[4]==0xffff);
    assert(pixels[2]==0x1234&&pixels[5]==0x5678);
    assert(surface.pixel_count==4&&surface.run_count==2);
    surface.clip_active=true;
    surface.clip_x0=surface.clip_y0=1;
    raylib_lite_primitive_draw_triangle(&surface,0,0,4,0,0,4,
        (raylib_lite_renderer_color_t){0,0,0,255});
    assert(pixels[4]==0&&pixels[0]==0xffff);
    assert(surface.pixel_count==5&&surface.run_count==3);
    raylib_lite_primitive_paint_t paint=raylib_lite_primitive_paint(
        (raylib_lite_renderer_color_t){255,0,0,255});
    raylib_lite_primitive_draw_span(&surface,1,-5,5,&paint);
    assert(pixels[4]==0xf800 && pixels[2]==0x1234 && pixels[5]==0x5678);
    assert(surface.pixel_count==6 && surface.run_count==4);
    raylib_lite_primitive_draw_span(&surface,0,0,2,&paint);
    paint.alpha=0;
    raylib_lite_primitive_draw_span(&surface,1,0,2,&paint);
    assert(surface.pixel_count==6 && surface.run_count==4);
    surface.pixels=NULL;
    raylib_lite_primitive_draw_rectangle(&surface,0,0,2,2,
        (raylib_lite_renderer_color_t){255,255,255,255});
    raylib_lite_primitive_draw_triangle(&surface,0,0,4,0,0,4,
        (raylib_lite_renderer_color_t){255,255,255,255});
    assert(surface.pixel_count==6&&surface.run_count==4);
}

int main(void)
{
    raylib_lite_renderer_texture_t invalid =
        raylib_lite_renderer_load_texture("invalid");
    assert(invalid.id == 0);
    assert(s_open_count == 1 && s_release_count == 1);

    raylib_lite_renderer_texture_t texture =
        raylib_lite_renderer_load_texture("texture");
    assert(texture.id && texture.width == 2 && texture.height == 2);
    assert(s_open_count == 2 && s_release_count == 1);
    raylib_lite_renderer_unload_texture(texture);
    assert(s_release_count == 2);
    raylib_lite_renderer_unload_texture(texture);
    assert(s_release_count == 2);

    uint16_t pixels[4] = {5, 6, 7, 8};
    raylib_lite_renderer_texture_t registered =
        raylib_lite_renderer_register_rgb565(pixels, 2, 2);
    assert(registered.id);
    raylib_lite_renderer_unload_texture(registered);
    assert(s_release_count == 2);

    raylib_lite_wall_atlas_t walls[20] = {0};
    for (size_t i = 0; i < 20; ++i) {
        walls[i] = raylib_lite_wall_atlas_load("wall");
        assert(walls[i].descriptor == &s_wall);
    }
    assert(s_open_count == 22 && s_release_count == 2);

    for (size_t i = 0; i < 20; ++i)
        raylib_lite_wall_atlas_unload(walls[i]);
    assert(s_release_count == 22);

    packaged_texture_contract();
    primitive_surface_is_borrowed_and_neutral();

    puts("renderer core: ok");
    return 0;
}
