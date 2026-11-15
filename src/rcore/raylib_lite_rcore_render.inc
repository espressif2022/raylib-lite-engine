// SPDX-License-Identifier: Apache-2.0
/* Included by raylib_lite_rcore.c with access to rlsw's implementation.
 * All public calls use raylib coordinates and handles. */

void raylib_lite_rcore_buffer_sizes(size_t *color_bytes, size_t *depth_bytes)
{
    *color_bytes = (size_t)RLSW.framebuffer.color.allocSz;
    *depth_bytes = (size_t)RLSW.framebuffer.depth.allocSz;
}

static bool default_2d_viewport(int width, int height)
{
    const sw_matrix_t identity = { 1, 0, 0, 0, 0, 1, 0, 0,
                                   0, 0, 1, 0, 0, 0, 0, 1 };
    const sw_matrix_t projection = { 2.0f/width, 0, 0, 0, 0, -2.0f/height, 0, 0,
                                     0, 0, -2, 0, -1, 1, -1, 1 };
    return RLSW.vpSize[0] == width && RLSW.vpSize[1] == height &&
           RLSW.vpCenter[0] == RLSW.vpHalf[0] && RLSW.vpCenter[1] == RLSW.vpHalf[1] &&
           memcmp(RLSW.stackModelview[RLSW.stackModelviewCounter - 1], identity, sizeof(identity)) == 0 &&
           memcmp(RLSW.stackTexture[RLSW.stackTextureCounter - 1], identity, sizeof(identity)) == 0 &&
           memcmp(RLSW.stackProjection[RLSW.stackProjectionCounter - 1],
                  projection, sizeof(projection)) == 0;
}

bool raylib_lite_rcore_blit_opaque(Texture2D texture, Rectangle source, Rectangle dest)
{
    sw_texture_t *target = &RLSW.framebuffer.color;
    if (RLSW.colorBuffer != target || !sw_is_texture_complete(target) ||
        target->format != SW_PIXELFORMAT_COLOR_R5G6B5 ||
        RLSW.polyMode != SW_FILL || sw_immediate_is_active() ||
        (RLSW.userState & (SW_STATE_SCISSOR_TEST | SW_STATE_DEPTH_TEST)) ||
        ((RLSW.userState & SW_STATE_CULL_FACE) && RLSW.cullFace != SW_BACK) ||
        ((RLSW.userState & SW_STATE_BLEND) &&
         RLSW.blendFunc != sw_blend_SRC_ALPHA_ONE_MINUS_SRC_ALPHA)) return false;

    sw_texture_t *input = sw_pool_get(&RLSW.texturePool, texture.id);
    bool flip_y = source.height < 0;
    if (!sw_is_texture_complete(input) || input->format != SW_PIXELFORMAT_COLOR_R5G6B5 ||
        input->pixels == target->pixels ||
        input->width != texture.width || input->height != texture.height ||
        input->magFilter != SW_NEAREST || input->minFilter != SW_NEAREST ||
        source.x != 0 || source.y != 0 || source.width != input->width ||
        (flip_y ? -source.height : source.height) != input->height) return false;

    if (!(dest.x >= 0 && dest.y >= 0 && dest.width >= input->width && dest.height >= input->height &&
          dest.width <= target->width - dest.x && dest.height <= target->height - dest.y)) return false;
    int x = (int)dest.x;
    int y = (int)dest.y;
    int width = (int)dest.width;
    int height = (int)dest.height;
    if (x != dest.x || y != dest.y || width != dest.width || height != dest.height ||
        width % input->width || height % input->height) return false;
    int scale = width/input->width;
    if (height/input->height != scale || !default_2d_viewport(target->width, target->height)) return false;

    /* The screen buffer is bottom-up. A negative source height samples a
     * render texture bottom-up too, so their row order already matches. */
    const uint16_t *pixels = (const uint16_t *)input->pixels;
    uint16_t *output = (uint16_t *)target->pixels;
    int bottom = target->height - y - height;
    size_t row_bytes = (size_t)width*sizeof(*output);
    for (int row = 0; row < input->height; row++)
    {
        int source_y = flip_y ? row : input->height - 1 - row;
        const uint16_t *src = pixels + (size_t)source_y*input->width;
        uint16_t *dst = output + (size_t)(bottom + row*scale)*target->width + x;
        if (scale == 1) memcpy(dst, src, row_bytes);
        else
        {
            uint16_t *pixel = dst;
            for (int column = 0; column < input->width; column++)
            {
                uint16_t color = src[column];
                for (int repeat = 0; repeat < scale; repeat++) *pixel++ = color;
            }
        }
        for (int repeat = 1; repeat < scale; repeat++)
            memcpy(dst + (size_t)repeat*target->width, dst, row_bytes);
    }
    rlColor4ub(255, 255, 255, 255);
    rlTexCoord2f(1, flip_y ? 1 : 0);
    rlSetTexture(0);
    return true;
}

static int clip_coordinate(int64_t value, int limit)
{
    if (value < 0) return 0;
    if (value > limit) return limit;
    return (int)value;
}

void BeginScissorMode(int x, int y, int width, int height)
{
    if (!RLSW.colorBuffer) return;
    int target_width = RLSW.colorBuffer->width;
    int target_height = RLSW.colorBuffer->height;
    int left = clip_coordinate(x, target_width);
    int top = clip_coordinate(y, target_height);
    int right = clip_coordinate((int64_t)x + width, target_width);
    int bottom = clip_coordinate((int64_t)y + height, target_height);

    rlDrawRenderBatchActive();
    rlEnableScissorTest();
    if (left >= right || top >= bottom)
    {
        /* Reject every primitive, including a line on a degenerate edge. */
        rlScissor(target_width, target_height, 0, 0);
        RLSW.scClipMin[0] = 2.0f;
        return;
    }

    /* rlsw computes clipping planes from top-left coordinates. Clears and
     * point tests use the raw bottom-up pixel coordinates instead. */
    rlScissor(left, top, right - left, bottom - top);
    RLSW.scMin[1] = target_height - bottom;
    RLSW.scMax[1] = target_height - top;
}

void ClearBackground(Color color)
{
    if (RLSW.userState & SW_STATE_SCISSOR_TEST)
    {
        if (RLSW.scMin[0] >= RLSW.scMax[0] || RLSW.scMin[1] >= RLSW.scMax[1])
            return;
        /* rlsw's clear loop uses inclusive maxima. */
        RLSW.scMax[0]--;
        RLSW.scMax[1]--;
        raylib_lite_upstream_clear_background(color);
        RLSW.scMax[0]++;
        RLSW.scMax[1]++;
        return;
    }
    raylib_lite_upstream_clear_background(color);
}

static void subtract_blend(float *dst, const float *src)
{
    for (int component = 0; component < 4; component++)
        dst[component] = sw_saturate(src[component] - dst[component]);
}

void BeginBlendMode(int mode)
{
    rlDrawRenderBatchActive();
    switch (mode)
    {
        case BLEND_ALPHA: swBlendFunc(SW_SRC_ALPHA, SW_ONE_MINUS_SRC_ALPHA); break;
        case BLEND_ADDITIVE: swBlendFunc(SW_SRC_ALPHA, SW_ONE); break;
        case BLEND_MULTIPLIED: swBlendFunc(SW_DST_COLOR, SW_ONE_MINUS_SRC_ALPHA); break;
        case BLEND_ADD_COLORS: swBlendFunc(SW_ONE, SW_ONE); break;
        case BLEND_SUBTRACT_COLORS:
            swBlendFunc(SW_ONE, SW_ONE);
            RLSW.blendFunc = subtract_blend;
            break;
        case BLEND_ALPHA_PREMULTIPLY: swBlendFunc(SW_ONE, SW_ONE_MINUS_SRC_ALPHA); break;
        default:
            TRACELOG(LOG_WARNING, "BLEND: Mode %d is unavailable in the software renderer", mode);
            swBlendFunc(SW_SRC_ALPHA, SW_ONE_MINUS_SRC_ALPHA);
            break;
    }
}

void EndBlendMode(void)
{
    BeginBlendMode(BLEND_ALPHA);
}

RenderTexture2D LoadRenderTexture(int width, int height)
{
    if (!IsWindowReady() || width <= 0 || height <= 0 ||
        width > INT_MAX/SW_FRAMEBUFFER_DEPTH_SIZE/height ||
        width > INT_MAX/SW_FRAMEBUFFER_COLOR_SIZE/height) return (RenderTexture2D){0};

    unsigned int previous = RLSW.boundFramebufferId;
    RenderTexture2D target = { .id = rlLoadFramebuffer() };
    if (!target.id) return target;

    target.texture = (Texture2D){
        .id = rlLoadTexture(NULL, width, height, PIXELFORMAT_UNCOMPRESSED_R5G6B5, 1),
        .width = width, .height = height, .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R5G6B5,
    };
    target.depth = (Texture2D){
        .id = rlLoadTextureDepth(width, height, true),
        .width = width, .height = height, .mipmaps = 1,
        .format = GL_DEPTH_COMPONENT32,
    };
    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH,
                        RL_ATTACHMENT_RENDERBUFFER, 0);
    if (!target.depth.id || !rlFramebufferComplete(target.id))
    {
        UnloadRenderTexture(target);
        target = (RenderTexture2D){0};
    }
    swBindFramebuffer(previous);
    return target;
}

bool IsRenderTextureValid(RenderTexture2D target)
{
    const sw_framebuffer_t *framebuffer = sw_pool_get(&RLSW.framebufferPool, target.id);
    if (!framebuffer || !target.texture.id || !target.depth.id ||
        target.texture.id != framebuffer->colorAttachment ||
        target.depth.id != framebuffer->depthAttachment) return false;
    sw_texture_t *color = sw_pool_get(&RLSW.texturePool, target.texture.id);
    sw_texture_t *depth = sw_pool_get(&RLSW.texturePool, target.depth.id);
    return sw_is_texture_complete(color) && sw_is_texture_complete(depth) &&
        color->format == SW_FRAMEBUFFER_COLOR_FORMAT && depth->format == SW_FRAMEBUFFER_DEPTH_FORMAT &&
        color->width == target.texture.width && color->height == target.texture.height &&
        depth->width == color->width && depth->height == color->height;
}

void UnloadRenderTexture(RenderTexture2D target)
{
    const sw_framebuffer_t *framebuffer = sw_pool_get(&RLSW.framebufferPool, target.id);
    if (!framebuffer || target.texture.id != framebuffer->colorAttachment ||
        target.depth.id != framebuffer->depthAttachment) return;

    if (RLSW.boundFramebufferId == target.id) EndTextureMode();
    swDeleteFramebuffers(1, &target.id);
    if (target.texture.id) swDeleteTextures(1, &target.texture.id);
    if (target.depth.id) swDeleteTextures(1, &target.depth.id);
}

Image LoadImageFromTexture(Texture2D texture)
{
    sw_texture_t *source = sw_pool_get(&RLSW.texturePool, texture.id);
    if (!sw_is_texture_complete(source) || source->width != texture.width ||
        source->height != texture.height) return (Image){0};
    unsigned int internal_format, format, type;
    rlGetGlTextureFormats(texture.format, &internal_format, &format, &type);
    if (!internal_format || sw_pixel_get_format(format, type) != (int)source->format)
        return (Image){0};
    int size = GetPixelDataSize(texture.width, texture.height, texture.format);
    if (size <= 0 || size > source->allocSz) return (Image){0};
    void *pixels = MemAlloc((unsigned int)size);
    if (!pixels) return (Image){0};
    memcpy(pixels, source->pixels, (size_t)size);
    return (Image){pixels, texture.width, texture.height, 1, texture.format};
}

Image LoadImageFromScreen(void)
{
    sw_texture_t *source = &RLSW.framebuffer.color;
    if (!sw_is_texture_complete(source) || source->width > INT_MAX/4/source->height)
        return (Image){0};
    void *pixels = MemAlloc((unsigned int)(4*source->width*source->height));
    if (!pixels) return (Image){0};
    sw_framebuffer_output_copy(pixels, source, 0, 0, source->width, source->height,
                              SW_PIXELFORMAT_COLOR_R8G8B8A8);
    return (Image){pixels, source->width, source->height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}

void TakeScreenshot(const char *fileName)
{
    if (!fileName) return;
    Image image = LoadImageFromScreen();
    if (!ExportImage(image, fileName))
        TRACELOG(LOG_WARNING, "SCREENSHOT: Could not save '%s'", fileName);
    UnloadImage(image);
}
