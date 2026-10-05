# SPDX-License-Identifier: Apache-2.0
"""Check scanline rounding and direct texture sampling against scalar oracles."""
from pathlib import Path
import os
import ctypes
import importlib.util
import subprocess
import shlex
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class FixedRasterTests(unittest.TestCase):
    def test_rounding_boundaries(self):
        source = r'''#include <assert.h>
#include "raylib_lite_renderer.c"
#include "raylib_lite_renderer_raylib.c"
int main(void) {
    const unsigned fractions[] = {0,1,32766,32767,32768,32769,32770,65534,65535};
    for (int integer = -32768; integer <= 32767; ++integer)
        for (unsigned i=0; i<sizeof(fractions)/sizeof(fractions[0]); ++i) {
            int32_t x=(int32_t)((int64_t)integer*65536+fractions[i]);
            int64_t t=(int64_t)x-32768;
            int expected=t>=0?(int)((t+65535)>>16):(int)(-((-t)>>16));
            assert(raster_ceil_fixed(x)==expected);
        }
    uint16_t texture[64*64], dst[66];
    for(unsigned i=0;i<64*64;++i)texture[i]=(uint16_t)(i*997U+123U);
    const int steps[]={-32768,-17001,-1,0,1,17001,32768};
    for(unsigned light=0;light<=256;light+=16)
    for(unsigned a=0;a<7;++a)for(unsigned b=0;b<7;++b)
    for(int count=0;count<=64;++count){
        int u=32*65536+123,v=32*65536+456;
        memset(dst,0xa5,sizeof(dst));
        if(light==256)fill_direct_unshaded(dst+1,texture,64,u,v,steps[a],steps[b],count);
        else fill_direct_shaded(dst+1,texture,64,u,v,steps[a],steps[b],count,light);
        for(int i=0;i<count;++i){
            unsigned p=texture[(v>>16)*64+(u>>16)];
            unsigned r=((p>>11)&31U)*light>>8;
            unsigned g=((p>>5)&63U)*light>>8;
            unsigned blue=(p&31U)*light>>8;
            assert(dst[i+1]==(uint16_t)((r<<11)|(g<<5)|blue));
            u+=steps[a];v+=steps[b];
        }
        assert(dst[0]==0xa5a5);
        for(int i=count+1;i<66;++i)assert(dst[i]==0xa5a5);
    }
    Texture2D tex=raylib_lite_2d_register_rgb565(texture,64,64);
    assert(tex.id);
    uint16_t before[33*32], after[33*32];
    for(unsigned light=0;light<=256;light+=16)for(int mirror=0;mirror<2;++mirror){
        raylib_lite_textured_vertex_t a={-3,1,mirror?-70:0,3,0},
            b={29,4,mirror?100:62,2,0},c={14,36,30,mirror?-80:60,0};
        assert(raylib_lite_2d_cache_texture_light(tex,256));
        memset(before,0xa5,sizeof before);memset(after,0xa5,sizeof after);
        raylib_lite_renderer_set_target(before,33,32,32);
        raylib_lite_2d_draw_textured_triangle(tex,a,b,c,light);
        assert(raylib_lite_2d_cache_texture_light(tex,light));
        raylib_lite_renderer_set_target(after,33,32,32);
        raylib_lite_2d_draw_textured_triangle(tex,a,b,c,light);
        assert(!memcmp(before,after,sizeof before));
    }
    assert(raylib_lite_2d_cache_texture_light(tex,232));
    assert(texture_slot((raylib_lite_renderer_texture_t){.id=tex.id,.width=tex.width,.height=tex.height})->cached_light==240);
    assert(raylib_lite_2d_cache_texture_light(tex,256));
    assert(!texture_slot((raylib_lite_renderer_texture_t){.id=tex.id,.width=tex.width,.height=tex.height})->light_cache);
    assert(!raylib_lite_2d_cache_texture_light((Texture2D){0},160));
    raylib_lite_2d_unload_texture(tex);
    raylib_lite_renderer_set_target(NULL,0,0,0);
    return 0;
}
'''
        spec = importlib.util.spec_from_file_location("host_run_game", ROOT / "host/run_game.py")
        host = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(host)
        checks = [f'_Static_assert(sizeof(raylib_lite_renderer_raster_stats_t)=={ctypes.sizeof(host.RasterStats)}, "stats size");']
        for name, _ in host.RasterStats._fields_:
            checks.append(f'_Static_assert(offsetof(raylib_lite_renderer_raster_stats_t,{name})=={getattr(host.RasterStats,name).offset}, "{name} offset");')
        source = source.replace('int main(void)', '\n'.join(checks)+'\nint main(void)')
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / "test.c").write_text(source)
            command = [os.environ.get("CC", "cc"), "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", str(p / "test.c"),
                       str(ROOT / "host/host_asset_runtime.c"), str(ROOT / "src/renderer/raylib_lite_rgb565.c")]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            for include in ["host", "host/include", "include/raylib_lite",
                            "include/raylib_lite",
                            "src/renderer", "include/raylib_lite"]:
                command += ["-I", str(ROOT / include)]
            subprocess.run(command + ["-lm", "-o", str(p / "test")], check=True)
            subprocess.run([str(p / "test")], check=True)


if __name__ == "__main__":
    unittest.main()
