// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_font.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *root;
static unsigned acquired, released;
/* Isolated file-asset adapter: every successful open has an owned view. This
 * catches lifetime errors without depending on either platform asset cache. */
raylib_lite_result_t raylib_lite_asset_open(const char *name, raylib_lite_asset_view_t *out)
{
    char path[1024];
    snprintf(path,sizeof(path),"%s/%s",root,name);
    FILE *file=fopen(path,"rb");
    if (!file) return RAYLIB_LITE_NOT_FOUND;
    assert(fseek(file,0,SEEK_END) == 0);
    long length=ftell(file);
    assert(length > 0 && fseek(file,0,SEEK_SET) == 0);
    uint8_t *bytes=malloc((size_t)length);
    assert(bytes && fread(bytes,1,(size_t)length,file) == (size_t)length);
    fclose(file);
    *out=(raylib_lite_asset_view_t){.data=bytes,.size=(size_t)length};
    ++acquired;
    return RAYLIB_LITE_OK;
}
void raylib_lite_asset_release(raylib_lite_asset_view_t *view)
{
    if (view && view->data) { free((void *)view->data); ++released; memset(view,0,sizeof(*view)); }
}

static void check_metrics(raylib_lite_font_t *font)
{
    raylib_lite_text_metrics_t m;
    assert(raylib_lite_font_measure(font,NULL,0,0,&m) == RAYLIB_LITE_OK);
    assert(!m.width && !m.height && !m.lines && !m.glyphs);
    const char text[]="A\xe4\xb8\xad\n\xe6\x96\x87?";
    assert(raylib_lite_font_measure(font,text,sizeof(text)-1,0,&m) == RAYLIB_LITE_OK);
    assert(m.width == 6 && m.height == 10 && m.lines == 2 && m.glyphs == 4);
    assert(!m.missing_glyphs && !m.invalid_utf8);
    assert(raylib_lite_font_measure(font,"A\r\nA\rA\n",7,0,&m) == RAYLIB_LITE_OK);
    assert(m.width == 3 && m.height == 20 && m.lines == 4 && m.glyphs == 3);
    assert(raylib_lite_font_measure(font,"AAA",3,4,&m) == RAYLIB_LITE_OK);
    assert(m.width == 3 && m.height == 15 && m.lines == 3);
    assert(raylib_lite_font_measure(font,"A",1,1,&m) == RAYLIB_LITE_OK);
    assert(m.width == 3 && m.height == 5 && m.lines == 1);
    /* Invalid leading byte, overlong, surrogate, out-of-range and truncated
     * sequences consume one byte each, independently of NUL termination. */
    const unsigned char bad[]={0x80,0xc0,0xaf,0xed,0xa0,0x80,0xf4,0x90,0x80,0x80,0xe4,0xb8};
    assert(raylib_lite_font_measure(font,(const char *)bad,sizeof(bad),0,&m) == RAYLIB_LITE_OK);
    assert(m.invalid_utf8 == sizeof(bad) && m.missing_glyphs == sizeof(bad));
    const unsigned char valid[]={0xf0,0x9f,0x98,0x80,0xc3,0xa9,0};
    assert(raylib_lite_font_measure(font,(const char *)valid,sizeof(valid),0,&m) == RAYLIB_LITE_OK);
    assert(m.glyphs == 3 && m.missing_glyphs == 3 && !m.invalid_utf8);
}

static void check_pixels(raylib_lite_font_t *font)
{
    uint16_t pixels[12*14]={0}, expected[12*14]={0};
    raylib_lite_renderer_set_target(pixels,12,12,14);
    raylib_lite_renderer_set_clip(2,2,4,7);
    const char text[]="A\xe4\xb8\xad\n\xe6\x96\x87?";
    raylib_lite_text_metrics_t drawn, measured;
    assert(raylib_lite_font_draw(font,text,sizeof(text)-1,1,1,0,
        (raylib_lite_renderer_color_t){255,255,255,255},&drawn) == RAYLIB_LITE_OK);
    assert(raylib_lite_font_measure(font,text,sizeof(text)-1,0,&measured) == RAYLIB_LITE_OK);
    assert(!memcmp(&drawn,&measured,sizeof(drawn)));
    /* Independently specified 2x3 masks for ?, A, 中, 文, and the four pen
     * positions. Pixels are tested with the real alpha renderer and clip. */
    const unsigned masks[]={0x15,0x2d,0x3f,0x36};
    const int glyphs[]={1,2,3,0}, px[]={1,4,1,4}, py[]={2,2,7,7};
    for (int n=0; n<4; ++n) for (int y=0; y<3; ++y) for (int x=0; x<2; ++x) {
        int dx=px[n]+x,dy=py[n]+y;
        if (dx>=2 && dx<6 && dy>=2 && dy<9 && (masks[glyphs[n]] & (1U<<(y*2+x))))
            expected[dy*12+dx]=65535;
    }
    assert(!memcmp(pixels,expected,sizeof(pixels)));
    assert(raylib_lite_font_draw(font,"A",1,0x7fffffff,0,0,
        (raylib_lite_renderer_color_t){255,255,255,255},NULL) == RAYLIB_LITE_INVALID_SIZE);
    assert(!memcmp(pixels,expected,sizeof(pixels)));
    raylib_lite_renderer_set_target(NULL,0,0,0);
}

static void check_validation(void)
{
    raylib_lite_asset_view_t view={0};
    assert(raylib_lite_asset_open("ui.font",&view) == RAYLIB_LITE_OK);
    uint8_t *storage=malloc(view.size+1), *data=storage+1;
    assert(storage);
    memcpy(data,view.data,view.size);
    raylib_lite_renderer_texture_t atlas=raylib_lite_renderer_load_texture("ui.atlas");
    assert(atlas.id);
    unsigned before=released;
    raylib_lite_font_t *borrowed=NULL;
    assert(raylib_lite_font_from_memory(data,view.size,atlas,&borrowed) == RAYLIB_LITE_OK);
    check_metrics(borrowed);
    raylib_lite_font_unload(borrowed);
    assert(released == before); /* borrowed data and texture remain live */
    assert(raylib_lite_font_from_memory(data,view.size-1,atlas,&borrowed) != RAYLIB_LITE_OK && !borrowed);
    for (size_t offset=0; offset<view.size; ++offset) {
        uint8_t saved=data[offset];
        data[offset]=255;
        raylib_lite_result_t result=raylib_lite_font_from_memory(data,view.size,atlas,&borrowed);
        if (result == RAYLIB_LITE_OK) raylib_lite_font_unload(borrowed);
        else assert(!borrowed);
        data[offset]=saved;
    }
    data[24+20]=data[24]; /* duplicate codepoint */
    assert(raylib_lite_font_from_memory(data,view.size,atlas,&borrowed) != RAYLIB_LITE_OK);
    memcpy(data,view.data,view.size);
    data[16]=0; /* fallback absent */
    assert(raylib_lite_font_from_memory(data,view.size,atlas,&borrowed) != RAYLIB_LITE_OK);
    free(storage);
    raylib_lite_renderer_unload_texture(atlas);
    raylib_lite_asset_release(&view);
    assert(acquired == released);
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    root=argv[1];
    raylib_lite_font_t *font=NULL;
    assert(raylib_lite_font_load("ui.font","ui.atlas",&font) == RAYLIB_LITE_OK);
    if (argc == 3) {
        /* Pipeline consumer: actual generated CJK subset, including a newline. */
        uint16_t pixels[128*64]={0};
        const char text[]="\xe4\xb8\xad\xe6\x96\x87\nA?";
        raylib_lite_renderer_set_target(pixels,128,128,64);
        raylib_lite_text_metrics_t m;
        assert(raylib_lite_font_draw(font,text,sizeof(text)-1,2,2,120,
            (raylib_lite_renderer_color_t){255,255,255,255},&m) == RAYLIB_LITE_OK);
        assert(m.glyphs == 4 && m.lines == 2 && !m.missing_glyphs);
        unsigned lit=0;
        for (size_t i=0; i<sizeof(pixels)/sizeof(*pixels); ++i) lit += pixels[i]!=0;
        assert(lit > 0);
        raylib_lite_renderer_set_target(NULL,0,0,0);
    } else { check_metrics(font); check_pixels(font); }
    raylib_lite_font_unload(font);
    assert(acquired == released);
    if (argc == 2) check_validation();
    assert(raylib_lite_font_load("ui.font","missing.atlas",&font) != RAYLIB_LITE_OK && !font);
    assert(acquired == released);
    raylib_lite_font_unload(NULL);
    puts("bitmap font: ok");
    return 0;
}
