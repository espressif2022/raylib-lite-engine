# SPDX-License-Identifier: Apache-2.0
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest
from tools.api_catalog import public_inventory

ROOT=Path(__file__).resolve().parents[1]


class RendererHeaderTests(unittest.TestCase):
    def compile_source(self, source, language='c', preprocess=False):
        compiler=os.environ.get('CC','cc') if language=='c' else os.environ.get('CXX','c++')
        self.assertIsNotNone(shutil.which(compiler))
        flags=['-std=c11' if language=='c' else '-std=c++17','-Wall','-Wextra','-Werror','-pedantic']
        flags+=shlex.split(os.environ.get('CFLAGS' if language=='c' else 'CXXFLAGS',''))
        flags+=['-I',str(ROOT/'include/raylib_lite'),'-x',language]
        flags+=['-E','-P'] if preprocess else ['-fsyntax-only']
        return subprocess.run([compiler]+flags+['-'],input=source,text=True,
                              capture_output=True,check=True).stdout

    def test_self_contained_c_and_cpp_contracts(self):
        for header in ['renderer_draw','renderer_stats','renderer_benchmark','renderer']:
            for language in ['c','c++']:
                with self.subTest(header=header,language=language):
                    self.compile_source(f'#include "raylib_lite_{header}.h"\n',language)
        source='''#include "raylib_lite_renderer.h"
#include <stddef.h>
#ifdef __cplusplus
#define ABI_ASSERT static_assert
#else
#define ABI_ASSERT _Static_assert
#endif
ABI_ASSERT(sizeof(raylib_lite_renderer_raster_stats_t)==184,"stats size");
ABI_ASSERT(offsetof(raylib_lite_renderer_raster_stats_t,sky_us)==88,"phase offset");
ABI_ASSERT(offsetof(raylib_lite_renderer_raster_stats_t,triangle_calls)==108,"triangle offset");
ABI_ASSERT(offsetof(raylib_lite_renderer_raster_stats_t,fb_pixels)==180,"framebuffer offset");
void declarations(void) {
 void (*draw)(raylib_lite_renderer_texture_t,raylib_lite_renderer_rect_t,
              raylib_lite_renderer_rect_t,raylib_lite_renderer_vec2_t,float,
              raylib_lite_renderer_color_t)=raylib_lite_renderer_draw_texture_pro;
 void (*get)(raylib_lite_renderer_raster_stats_t *)=raylib_lite_renderer_get_raster_stats;
 void (*phase)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t)=raylib_lite_renderer_set_phase_us;
 void (*benchmark)(void)=raylib_lite_renderer_run_benchmark;
 (void)draw;(void)get;(void)phase;(void)benchmark;
}
'''
        self.compile_source(source)
        self.compile_source(source,'c++')

    def test_drawing_and_stats_do_not_import_other_roles(self):
        drawing=self.compile_source('#include "raylib_lite_renderer_draw.h"\n',preprocess=True)
        for name in ['raylib_lite_renderer_raster_stats_t','raylib_lite_renderer_note_primitives',
                     'raylib_lite_renderer_run_benchmark','Texture2D','esp_err_t']:
            self.assertNotIn(name,drawing)
        stats=self.compile_source('#include "raylib_lite_renderer_stats.h"\n',preprocess=True)
        self.assertNotIn('raylib_lite_renderer_texture_t',stats)
        self.assertNotIn('raylib_lite_renderer_run_benchmark',stats)
        inventory=public_inventory()
        self.assertEqual(inventory['renderer']['entries'],[])
        self.assertEqual({e['name'] for e in inventory['renderer_benchmark']['entries']},
                         {'raylib_lite_renderer_run_benchmark'})

    def test_cpp_calls_c_definitions_through_legacy_umbrella(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            c=root/'provider.c';cpp=root/'consumer.cpp';obj=root/'provider.o';exe=root/'contract'
            c.write_text('''#include "raylib_lite_renderer_stats.h"
#include "raylib_lite_renderer_benchmark.h"
void raylib_lite_renderer_get_raster_stats(raylib_lite_renderer_raster_stats_t *out)
{ *out=(raylib_lite_renderer_raster_stats_t){0};out->fb_pixels=7; }
void raylib_lite_renderer_run_benchmark(void) {}
''')
            cpp.write_text('''#include "raylib_lite_renderer.h"
int main(){raylib_lite_renderer_raster_stats_t stats;
raylib_lite_renderer_get_raster_stats(&stats);raylib_lite_renderer_run_benchmark();
return stats.fb_pixels==7?0:1;}
''')
            includes=['-I',str(ROOT/'include/raylib_lite')]
            subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-pedantic']+
                           includes+['-c',str(c),'-o',str(obj)],check=True)
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-pedantic']+
                           includes+[str(cpp),str(obj),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)


if __name__=='__main__':unittest.main()
