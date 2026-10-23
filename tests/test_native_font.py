# SPDX-License-Identifier: Apache-2.0
"""Real native BMFont page loading, UTF-8 drawing, and repeated unload regression."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from PIL import Image

from tests.test_font import fixture_font
from tests.test_rcore_platform import ROOT, RAYLIB, DEFINES, INCLUDES


class NativeFontTests(unittest.TestCase):
    def test_native_bmfont_pages_and_utf8(self):
        cc = os.environ.get("CC", "cc")
        flags = shlex.split(os.environ.get("CFLAGS", ""))
        includes = [arg for path in [*INCLUDES, ROOT / "tools/frame_compare"]
                    for arg in ("-I", str(path))]
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            objects = []
            sources = [RAYLIB / "src" / name for name in ("rshapes.c", "rtextures.c", "rtext.c")]
            for source in sources + [ROOT / "src/rcore/raylib_lite_rcore.c"]:
                obj = output / (source.stem + ".o")
                subprocess.run([cc, "-std=gnu99", "-O1", "-w", *DEFINES,
                                "-DRAYLIB_LITE_RCORE_NO_ASSET_BRIDGE=1", *flags, *includes,
                                "-c", str(source), "-o", str(obj)], check=True)
                objects.append(str(obj))
            executable = output / "native_font"
            sources = [ROOT / path for path in (
                "tests/test_native_font.c", "tools/frame_compare/frame_host.c",
                "tools/frame_compare/frame_scene.c",
                "src/runtime/raylib_lite_raylib_port.c", "src/runner/raylib_lite_input_queue.c")]
            subprocess.run([cc, "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", *flags,
                            *includes, *map(str, sources), *objects, "-lm", "-o", str(executable)], check=True)
            colors = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255)]
            codepoints = [65, 0x4e2d, 66]
            for page, color in enumerate(colors):
                Image.new("RGBA", (8, 8), color).save(output / f"page{page}.png")
            paths = []
            for pages in (1, 2, 3):
                lines = ['info face="regression" size=8',
                         f"common lineHeight=8 base=8 scaleW=8 scaleH=8 pages={pages}"]
                lines += [f'page id={i} file="page{i}.png"' for i in range(pages)]
                lines += [f"chars count={pages}"]
                lines += [f"char id={codepoints[i]} x=0 y=0 width=4 height=4 "
                          f"xoffset=0 yoffset=0 xadvance=4 page={i}" for i in range(pages)]
                path = output / f"font{pages}.fnt"
                path.write_text("\n".join(lines) + "\n")
                paths.append(str(path))
            ttf = output / "source.ttf"
            fixture_font(ttf)
            txt = output / "strings.txt"
            txt.write_text("中文\nA", encoding="utf-8")
            packed = output / "packed_fb"
            subprocess.run(["python3", str(ROOT / "tools/pack_font.py"),
                            "--font", str(ttf), "--text-file", str(txt),
                            "--fallback", "U+4E2D", "--size", "12", "--atlas-width", "64",
                            "--license", "Original test fixture; Apache-2.0",
                            "--output", str(packed)], check=True)
            report = json.loads(Path(str(packed) + ".font.json").read_text(encoding="utf-8"))
            self.assertEqual(report["glyph_count"], 4)
            self.assertIn("chars count=4", packed.with_suffix(".fnt").read_text(encoding="utf-8"))
            subprocess.run([str(executable), *paths, str(packed.with_suffix(".fnt"))], check=True)


if __name__ == "__main__":
    unittest.main()
