# SPDX-License-Identifier: Apache-2.0
"""Default-font pixels and layout against the vendored raylib atlas."""
import math
from fractions import Fraction
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class CompatTextTests(unittest.TestCase):
    def test_default_font_pixels_and_measurement(self):
        upstream = (ROOT / 'third_party/raylib/src/rtext.c').read_text()
        words = [int(x, 16) for x in re.search(
            r'unsigned int defaultFontData\[512\] = \{(.*?)\};', upstream, re.S)[1].split(',') if x.strip()]
        widths = [int(x) for x in re.search(
            r'int charsWidth\[224\] = \{(.*?)\};', upstream, re.S)[1].split(',') if x.strip()]
        # Reconstruct the original atlas rectangles, independently of row masks.
        rects = []
        x, y = 1, 1
        for width in widths:
            if x + width + 1 >= 128:
                x, y = 1, y + 11
            rects.append((x, y, width))
            x += width + 1
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            (temp / 'text.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib_lite_host_video.h"
#include "raylib_lite_raylib.h"
static unsigned short pixels[256*96];
int main(int argc,char **argv){
 assert(argc==3);
 int size=atoi(argv[1]);
 assert(raylib_lite_host_video_set_target(pixels,256,256,96)==RAYLIB_LITE_OK);
 BeginDrawing();
 ClearBackground(BLACK);
 DrawText("Aa iW!?\nabc",3,2,size,WHITE);
 EndDrawing();
 printf("%d %d %d %d\n",MeasureText("Aa iW!?\nabc",size),
  MeasureText("\xc3\xa9",size),MeasureText("\xe4\xb8\xad",size),MeasureText("\xff",size));
 FILE *out=fopen(argv[2],"wb");assert(out);
 assert(fwrite(pixels,sizeof(pixels),1,out)==1);fclose(out);
 return 0;
}
''')
            sources = ['host/host_video_backend.c', 'src/runtime/raylib_lite_raylib_port.c',
                       'host/host_asset_runtime.c', 'src/renderer/raylib_lite_renderer.c',
                       'src/renderer/raylib_lite_renderer_raylib.c',
                       'src/renderer/raylib_lite_rgb565.c', 'src/renderer/raylib_lite_raylib_impl.c']
            command = [os.environ.get('CC', 'cc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
            command += shlex.split(os.environ.get('CFLAGS', ''))
            command += [str(temp / 'text.c')] + [str(ROOT / p) for p in sources]
            for include in ['host/include', 'host', 'include/raylib_lite', 'compat/raylib/include']:
                command += ['-I', str(ROOT / include)]
            subprocess.run(command + ['-lm', '-o', str(temp / 'text')], check=True)
            for size in [1, 10, 13, 20, 27]:
                result = subprocess.run([str(temp / 'text'), str(size), str(temp / 'pixels')],
                                        check=True, capture_output=True, text=True)
                size = max(size, 10)
                scale, spacing = Fraction(size, 10), size // 10
                lines = ['Aa iW!?', 'abc']
                measured = int(max(sum(widths[ord(c)-32] for c in line) for line in lines)*scale
                               + (max(map(len, lines))-1)*spacing)
                self.assertEqual(list(map(int, result.stdout.split())),
                                 [measured, int(widths[233-32]*scale),
                                  int(widths[ord('?')-32]*scale), int(widths[ord('?')-32]*scale)])
                expected = [0] * (256*96)
                for line_index, line in enumerate(lines):
                    left, top = Fraction(3), Fraction(2 + line_index*(size+2))
                    for char in line:
                        ax, ay, width = rects[ord(char)-32]
                        for y in range(max(0, math.ceil(top-Fraction(1, 2))), min(96, math.ceil(top+size-Fraction(1, 2)))):
                            for x in range(max(0, math.ceil(left-Fraction(1, 2))), min(256, math.ceil(left+width*scale-Fraction(1, 2)))):
                                sx, sy = int((Fraction(2*x+1, 2)-left)/scale), int((Fraction(2*y+1, 2)-top)/scale)
                                offset = (ay+sy)*128+ax+sx
                                if words[offset//32] & (1 << (offset%32)):
                                    expected[y*256+x] = 65535
                        left += width*scale+spacing
                actual = struct.unpack('=24576H', (temp / 'pixels').read_bytes())
                mismatches = [(i % 256, i // 256, a, b) for i, (a, b) in enumerate(zip(actual, expected)) if a != b]
                self.assertFalse(mismatches, f'font size {size}: {mismatches[:10]}')

    def test_generated_font_matches_upstream(self):
        subprocess.run(['python3', str(ROOT / 'tools/generate_compat_font.py'), '--check'], check=True)
