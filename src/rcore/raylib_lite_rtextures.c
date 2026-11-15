// SPDX-License-Identifier: Apache-2.0
/* Keep the vendored source intact. The software platform supplies the GPU
 * operations below. Export reuses the QOI encoder already built by rtextures. */
#define LoadRenderTexture raylib_lite_upstream_load_render_texture
#define IsRenderTextureValid raylib_lite_upstream_is_render_texture_valid
#define UnloadRenderTexture raylib_lite_upstream_unload_render_texture
#define LoadImageFromTexture raylib_lite_upstream_load_image_from_texture
#define LoadImageFromScreen raylib_lite_upstream_load_image_from_screen
#define DrawTexturePro raylib_lite_upstream_draw_texture_pro
#define ExportImage raylib_lite_upstream_export_image
#define ExportImageToMemory raylib_lite_upstream_export_image_to_memory
#include "rtextures.c"
#undef DrawTexturePro
#undef ExportImage
#undef ExportImageToMemory

#include <limits.h>
#include "raylib_lite_rcore_render.h"

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    if (texture.format == PIXELFORMAT_UNCOMPRESSED_R5G6B5 && rotation == 0 &&
        tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255)
    {
        Rectangle bounds = { dest.x - origin.x, dest.y - origin.y, dest.width, dest.height };
        if (raylib_lite_rcore_blit_opaque(texture, source, bounds)) return;
    }
    raylib_lite_upstream_draw_texture_pro(texture, source, dest, origin, rotation, tint);
}

unsigned char *ExportImageToMemory(Image image, const char *fileType, int *dataSize)
{
    if (!dataSize) return NULL;
    *dataSize = 0;
    if (!fileType || !TextIsEqual(TextToLower(fileType), ".qoi") || !IsImageValid(image) ||
        image.format >= PIXELFORMAT_COMPRESSED_DXT1_RGB ||
        image.width > INT_MAX/4/image.height) return NULL;

    Color *converted = NULL;
    const void *pixels = image.data;
    int channels = 4;
    if (image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8) channels = 3;
    else if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    {
        converted = LoadImageColors(image);
        if (!converted) return NULL;
        pixels = converted;
    }
    qoi_desc description = {
        .width = (unsigned int)image.width, .height = (unsigned int)image.height,
        .channels = channels, .colorspace = QOI_SRGB,
    };
    unsigned char *encoded = qoi_encode(pixels, &description, dataSize);
    UnloadImageColors(converted);
    return encoded;
}

bool ExportImage(Image image, const char *fileName)
{
    if (!fileName) return false;
    int size;
    unsigned char *data = ExportImageToMemory(image, GetFileExtension(fileName), &size);
    if (!data) return false;
    bool result = SaveFileData(fileName, data, size);
    MemFree(data);
    return result;
}
