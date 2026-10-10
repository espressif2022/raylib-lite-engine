# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
up=(root/'third_party/raylib/src/rtext.c').read_text()
data=[int(x,16) for x in re.search(r'unsigned int defaultFontData\[512\] = \{(.*?)\};',up,re.S)[1].split(',') if x.strip()]
widths=[int(x) for x in re.search(r'int charsWidth\[224\] = \{(.*?)\};',up,re.S)[1].split(',') if x.strip()]
rows=[]; line=0; pos=test=1
for w in widths:
 x=pos;y=1+line*11;test+=w+1
 if test>=128:
  line+=1;pos=test=2+w;x=1;y=1+line*11
 else:pos=test
 rows.append([sum(((data[(y+j)*4+(x+k)//32]>>((x+k)%32))&1)<<k for k in range(w)) for j in range(10)])
notice=up[up.index('*   LICENSE:'):up.index('**********************************************************************************************/')]
header='/*\n'+notice+'*/\n/* Adapted from raylib 6.0 default font: glyph-local row masks, not an atlas. */\n#include <stdint.h>\nstatic const uint8_t compat_font_width[224] = {\n'
header+='\n'.join('    '+', '.join(map(str,widths[i:i+32]))+',' for i in range(0,224,32))+'\n};\nstatic const uint16_t compat_font_rows[224][10] = {\n'
header+='\n'.join('    {'+', '.join(map(str,r))+'},' for r in rows)+'\n};\n'
# Benchmark oracle keeps the original atlas representation, independent of
# the renderer's glyph-local row masks.
reference='/*\n'+notice+'*/\n/* Adapted raylib default atlas for the independent benchmark oracle. */\n#include <stdint.h>\nstatic const uint32_t oracle_font_atlas[512] = {\n'
reference+='\n'.join('    '+', '.join(f'0x{x:08x}' for x in data[i:i+8])+',' for i in range(0,512,8))+'\n};\n'
reference+='static const uint8_t oracle_font_width[224] = {\n'
reference+='\n'.join('    '+', '.join(map(str,widths[i:i+32]))+',' for i in range(0,224,32))+'\n};\n'

if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Extract the vendored raylib default font without runtime allocation.')
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    outputs={root/'src/renderer/raylib_lite_default_font.h':header,
             root/'examples/render_benchmark/main/default_font_oracle.h':reference}
    for target, content in outputs.items():
        if args.check:
            if target.read_text()!=content:
                raise SystemExit(f'{target} differs from vendored raylib; regenerate it.')
        else:
            target.write_text(content)
