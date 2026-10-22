// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_font.h"
#include <stdlib.h>
#include <string.h>

#define FONT_HEADER 24U
#define FONT_GLYPH 20U
#define LAYOUT_LIMIT (1U << 23)
struct raylib_lite_font {
    const uint8_t *data;
    uint32_t count, fallback;
    uint16_t line_height;
    raylib_lite_renderer_texture_t atlas;
    raylib_lite_asset_view_t asset;
    bool owned;
};
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static int32_t i16(const uint8_t *p) { uint16_t v=u16(p); return v < 32768 ? v : (int32_t)v-65536; }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p+2) << 16; }
static bool scalar(uint32_t cp) { return cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff); }
static const uint8_t *glyph(const raylib_lite_font_t *font, uint32_t cp)
{
    uint32_t lo=0, hi=font->count;
    while (lo < hi) {
        uint32_t mid=lo+(hi-lo)/2;
        const uint8_t *p=font->data+FONT_HEADER+(size_t)mid*FONT_GLYPH;
        uint32_t value=u32(p);
        if (value == cp) return p;
        if (value < cp) lo=mid+1; else hi=mid;
    }
    return NULL;
}
raylib_lite_result_t raylib_lite_font_from_memory(const void *data, size_t size,
    raylib_lite_renderer_texture_t atlas, raylib_lite_font_t **out)
{
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out=NULL;
    if (!data || !atlas.id || size < FONT_HEADER) return RAYLIB_LITE_INVALID_ARGUMENT;
    const uint8_t *p=data;
    if (memcmp(p,"RLF1",4)) return RAYLIB_LITE_INVALID_VERSION;
    uint32_t count=u32(p+12), fallback=u32(p+16);
    uint16_t width=u16(p+4), height=u16(p+6), line=u16(p+8);
    if (!count || count > 65535 || size != FONT_HEADER+(size_t)count*FONT_GLYPH ||
        !width || !height || atlas.width != width || atlas.height != height ||
        !line || line > 4096 || u16(p+10) > line || u32(p+20) || !scalar(fallback))
        return RAYLIB_LITE_INVALID_ARGUMENT;
    uint32_t previous=0;
    bool found=false;
    for (uint32_t n=0; n<count; ++n) {
        const uint8_t *g=p+FONT_HEADER+(size_t)n*FONT_GLYPH;
        uint32_t cp=u32(g);
        if (!scalar(cp) || (n && cp <= previous) ||
            (uint32_t)u16(g+4)+u16(g+8) > width ||
            (uint32_t)u16(g+6)+u16(g+10) > height ||
            i16(g+12) < -4096 || i16(g+12) > 4096 ||
            i16(g+14) < -4096 || i16(g+14) > 4096 ||
            u16(g+16) > 4096 || u16(g+18)) return RAYLIB_LITE_INVALID_ARGUMENT;
        previous=cp;
        found |= cp == fallback;
    }
    if (!found) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_font_t *font=calloc(1,sizeof(*font));
    if (!font) return RAYLIB_LITE_NO_MEMORY;
    font->data=p; font->count=count; font->fallback=fallback;
    font->line_height=line; font->atlas=atlas;
    *out=font;
    return RAYLIB_LITE_OK;
}
raylib_lite_result_t raylib_lite_font_load(const char *metrics_asset,
    const char *atlas_asset, raylib_lite_font_t **out)
{
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out=NULL;
    if (!metrics_asset || !atlas_asset) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_asset_view_t view={0};
    raylib_lite_result_t result=raylib_lite_asset_open(metrics_asset,&view);
    if (result != RAYLIB_LITE_OK) return result;
    raylib_lite_renderer_texture_t texture=raylib_lite_renderer_load_texture(atlas_asset);
    if (!texture.id) { raylib_lite_asset_release(&view); return RAYLIB_LITE_NOT_READY; }
    result=raylib_lite_font_from_memory(view.data,view.size,texture,out);
    if (result != RAYLIB_LITE_OK) {
        raylib_lite_renderer_unload_texture(texture);
        raylib_lite_asset_release(&view);
    } else { (*out)->owned=true; (*out)->asset=view; }
    return result;
}
void raylib_lite_font_unload(raylib_lite_font_t *font)
{
    if (!font) return;
    if (font->owned) {
        raylib_lite_renderer_unload_texture(font->atlas);
        raylib_lite_asset_release(&font->asset);
    }
    free(font);
}
/* Reject overlong encodings, surrogates and values above Unicode's range.
 * On error consume exactly one byte, independent of the following input. */
static uint32_t decode(const uint8_t **cursor, const uint8_t *end, bool *invalid)
{
    const uint8_t *p=*cursor;
    unsigned n;
    uint32_t value, minimum;
    *invalid=false;
    if (*p < 0x80) { *cursor=p+1; return *p; }
    if (*p >= 0xc2 && *p <= 0xdf) { n=2; value=*p & 31; minimum=0x80; }
    else if (*p >= 0xe0 && *p <= 0xef) { n=3; value=*p & 15; minimum=0x800; }
    else if (*p >= 0xf0 && *p <= 0xf4) { n=4; value=*p & 7; minimum=0x10000; }
    else goto invalid;
    if ((size_t)(end-p) < n) goto invalid;
    for (unsigned i=1; i<n; ++i) {
        if ((p[i] & 0xc0) != 0x80) goto invalid;
        value=(value<<6) | (p[i]&63);
    }
    if (value < minimum || !scalar(value)) goto invalid;
    *cursor=p+n; return value;
invalid:
    *cursor=p+1; *invalid=true; return 0xfffd;
}
static raylib_lite_result_t layout(const raylib_lite_font_t *font,
    const char *text, size_t length, uint32_t wrap, bool draw, int x, int y,
    raylib_lite_renderer_color_t color, raylib_lite_text_metrics_t *out)
{
    memset(out,0,sizeof(*out));
    if (!font || (!text && length) || length > LAYOUT_LIMIT) return RAYLIB_LITE_INVALID_ARGUMENT;
    if (!length) return RAYLIB_LITE_OK;
    const uint8_t *p=(const uint8_t *)text, *end=p+length;
    uint32_t pen=0, row=0;
    out->lines=1;
    while (p < end) {
        bool invalid;
        uint32_t cp=decode(&p,end,&invalid);
        out->invalid_utf8 += invalid;
        if (!invalid && (cp == '\r' || cp == '\n')) {
            if (cp == '\r' && p < end && *p == '\n') ++p;
            if (pen > out->width) out->width=pen;
            pen=0; row+=font->line_height; ++out->lines;
        } else {
            const uint8_t *g=invalid ? NULL : glyph(font,cp);
            if (!g) { g=glyph(font,font->fallback); ++out->missing_glyphs; }
            uint32_t advance=u16(g+16);
            if (wrap && pen && pen+advance > wrap) {
                if (pen > out->width) out->width=pen;
                pen=0; row+=font->line_height; ++out->lines;
            }
            if (row > LAYOUT_LIMIT || pen+advance > LAYOUT_LIMIT) return RAYLIB_LITE_INVALID_SIZE;
            if (draw && u16(g+8) && u16(g+10)) {
                raylib_lite_renderer_draw_texture_pro(font->atlas,
                    (raylib_lite_renderer_rect_t){u16(g+4),u16(g+6),u16(g+8),u16(g+10)},
                    (raylib_lite_renderer_rect_t){(float)x+pen+i16(g+12),
                        (float)y+row+i16(g+14),u16(g+8),u16(g+10)},
                    (raylib_lite_renderer_vec2_t){0,0},0,color);
            }
            pen+=advance; ++out->glyphs;
        }
        if (row+font->line_height > LAYOUT_LIMIT) return RAYLIB_LITE_INVALID_SIZE;
    }
    if (pen > out->width) out->width=pen;
    out->height=row+font->line_height;
    return RAYLIB_LITE_OK;
}
raylib_lite_result_t raylib_lite_font_measure(const raylib_lite_font_t *font,
    const char *text, size_t byte_count, uint32_t wrap_width,
    raylib_lite_text_metrics_t *out)
{
    if (!out) return RAYLIB_LITE_INVALID_ARGUMENT;
    return layout(font,text,byte_count,wrap_width,false,0,0,
        (raylib_lite_renderer_color_t){0},out);
}
raylib_lite_result_t raylib_lite_font_draw(const raylib_lite_font_t *font,
    const char *text, size_t byte_count, int x, int y, uint32_t wrap_width,
    raylib_lite_renderer_color_t color, raylib_lite_text_metrics_t *out)
{
    raylib_lite_text_metrics_t measured;
    raylib_lite_result_t result=raylib_lite_font_measure(font,text,byte_count,wrap_width,&measured);
    if (out) *out=measured;
    if (result != RAYLIB_LITE_OK) return result;
    if ((int64_t)x < -(int64_t)LAYOUT_LIMIT+4096 ||
        (int64_t)y < -(int64_t)LAYOUT_LIMIT+4096 ||
        (int64_t)x+measured.width+65535+4096 > LAYOUT_LIMIT ||
        (int64_t)y+measured.height+65535+4096 > LAYOUT_LIMIT) return RAYLIB_LITE_INVALID_SIZE;
    return layout(font,text,byte_count,wrap_width,true,x,y,color,&measured);
}
