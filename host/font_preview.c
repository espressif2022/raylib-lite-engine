// SPDX-License-Identifier: Apache-2.0
/* Small neutral-API consumer: packed assets -> shared Host store -> layout ->
 * RGB565 render -> PPM. Does not depend on the raylib compatibility facade. */
#include "raylib_lite_font.h"
#include "host_asset_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr,"usage: font_preview ASSET_DIR TEXT_FILE OUTPUT.ppm WIDTH HEIGHT\n");
        return 2;
    }
    char *end;
    long width=strtol(argv[4],&end,10);
    if (*end || width < 48 || width > 4096) return 2;
    long height=strtol(argv[5],&end,10);
    if (*end || height < 48 || height > 4096) return 2;
    int status=1;
    raylib_lite_font_t *font=NULL;
    uint16_t *pixels=NULL;
    char *text=NULL;
    FILE *input=fopen(argv[2],"rb"), *output=NULL;
    if (!input) { perror(argv[2]); goto cleanup; }
    if (fseek(input,0,SEEK_END)) goto cleanup;
    long length=ftell(input);
    if (length < 0 || length > 1024*1024 || fseek(input,0,SEEK_SET)) goto cleanup;
    text=malloc((size_t)length+1);
    pixels=calloc((size_t)width*(size_t)height,sizeof(*pixels));
    if (!text || !pixels || fread(text,1,(size_t)length,input) != (size_t)length) goto cleanup;
    raylib_lite_host_assets_set_root(argv[1]);
    raylib_lite_result_t result=raylib_lite_font_load("ui.font","ui.atlas",&font);
    if (result != RAYLIB_LITE_OK) { fprintf(stderr,"font load failed: %d\n",result); goto cleanup; }
    for (long i=0; i<width*height; ++i) pixels[i]=0x0843;
    raylib_lite_renderer_set_target(pixels,(size_t)width,(int)width,(int)height);
    raylib_lite_renderer_set_clip(16,16,(int)width-32,(int)height-32);
    raylib_lite_text_metrics_t measured,drawn;
    result=raylib_lite_font_measure(font,text,(size_t)length,(uint32_t)width-32,&measured);
    if (result != RAYLIB_LITE_OK) goto cleanup;
    result=raylib_lite_font_draw(font,text,(size_t)length,16,16,(uint32_t)width-32,
        (raylib_lite_renderer_color_t){230,245,255,255},&drawn);
    if (result != RAYLIB_LITE_OK || memcmp(&measured,&drawn,sizeof(measured))) goto cleanup;
    output=fopen(argv[3],"wb");
    if (!output) { perror(argv[3]); goto cleanup; }
    if (fprintf(output,"P6\n%ld %ld\n255\n",width,height) < 0) goto cleanup;
    for (long i=0; i<width*height; ++i) {
        unsigned r=(pixels[i]>>11)&31,g=(pixels[i]>>5)&63,b=pixels[i]&31;
        unsigned char rgb[]={(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)};
        if (fwrite(rgb,1,3,output) != 3) goto cleanup;
    }
    if (fclose(output)) { output=NULL; goto cleanup; }
    output=NULL;
    printf("{\"width\":%u,\"height\":%u,\"lines\":%u,\"glyphs\":%u,"
           "\"missing_glyphs\":%u,\"invalid_utf8\":%u}\n",
           measured.width,measured.height,measured.lines,measured.glyphs,
           measured.missing_glyphs,measured.invalid_utf8);
    status=0;
cleanup:
    if (input) fclose(input);
    if (output) fclose(output);
    raylib_lite_renderer_set_target(NULL,0,0,0);
    raylib_lite_font_unload(font);
    raylib_lite_assets_unmount();
    free(pixels); free(text);
    return status;
}
