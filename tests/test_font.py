# SPDX-License-Identifier: Apache-2.0
"""Independent BMFont layout/pixel oracle and reproducible offline CJK subset."""
from pathlib import Path
import json
import os
import shlex
import struct
import subprocess
import tempfile
import unittest

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

ROOT = Path(__file__).resolve().parents[1]


def fixture_font(path: Path) -> None:
    # Original geometric test outlines, generated locally; no third-party font.
    builder = FontBuilder(1000, isTTF=True)
    names = [".notdef", "question", "A", "zhong", "wen", "space"]
    builder.setupGlyphOrder(names)
    builder.setupCharacterMap({63: "question", 65: "A", 0x4E2D: "zhong", 0x6587: "wen", 32: "space"})
    glyphs = {}
    for n, name in enumerate(names):
        pen = TTGlyphPen(None)
        if name != "space":
            pen.moveTo((50, 0)); pen.lineTo((150 + n * 60, 0))
            pen.lineTo((150 + n * 60, 700)); pen.lineTo((50, 700)); pen.closePath()
        glyphs[name] = pen.glyph()
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics({name: (600, 50) for name in names})
    builder.setupHorizontalHeader(ascent=800, descent=-200)
    builder.setupNameTable({"familyName": "Original Test Font", "styleName": "Regular",
                           "uniqueFontIdentifier": "raylib-lite-test-font",
                           "fullName": "Original Test Font", "psName": "OriginalTestFont"})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    builder.setupMaxp()
    builder.save(path)


class FontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temp.name)
        cls.executable = cls.directory / "font-test"
        command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
        command += shlex.split(os.environ.get("CFLAGS", ""))
        for inc in ["host/include", "include/raylib_lite", "compat/raylib/include", "src/renderer"]:
            command += ["-I", str(ROOT / inc)]
        command += [str(ROOT / source) for source in [
            "tests/test_font.c",
            "host/host_clock.c",
            "src/runtime/raylib_lite_raylib_port.c",
            "src/renderer/raylib_lite_renderer.c",
            "src/renderer/raylib_lite_renderer_raylib.c",
            "src/renderer/raylib_lite_raylib_impl.c",
            "src/renderer/raylib_lite_rgb565.c"]]
        subprocess.run(command + ["-lm", "-o", str(cls.executable)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_layout_utf8_lifetime_and_pixels(self):
        folder = self.directory / "oracle"
        folder.mkdir()
        lines = [
            'info face="oracle" size=5',
            "common lineHeight=5 base=4 scaleW=8 scaleH=3 pages=1",
            'page id=0 file="ui.png"',
            "chars count=4",
        ]
        # Offline fallback alias: char id=63 points to U+4E2D (中, x=4) so both
        # native raylib and compat LoadFont use U+4E2D for unknown codepoints.
        for n, cp in enumerate([63, 65, 0x4E2D, 0x6587]):
            gx = 4 if cp == 63 else n * 2
            lines.append(f"char id={cp} x={gx} y=0 width=2 height=3 xoffset=0 yoffset=1 xadvance=3 page=0")
        (folder / "ui.fnt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        masks = [0x15, 0x2D, 0x3F, 0x36]
        alpha = bytes(255 if masks[x // 2] & (1 << (y * 2 + x % 2)) else 0
                      for y in range(3) for x in range(8))
        (folder / "ui.atlas").write_bytes(struct.pack("<4sHHHHII", b"MSA1", 8, 3, 0, 1, 48, 24)
                                          + b"\xff\xff" * 24 + alpha)
        subprocess.run([str(self.executable), str(folder)], check=True)

    def pack(self, folder, *extra):
        folder.mkdir(exist_ok=True)
        font = self.directory / "source.ttf"
        if not font.exists():
            fixture_font(font)
        text = self.directory / "strings.txt"
        text.write_text("中文\nA? ", encoding="utf-8")
        return subprocess.run(["python3", str(ROOT / "tools/pack_font.py"), "--font", str(font),
            "--text-file", str(text), "--fallback", "U+4E2D", "--size", "16", "--atlas-width", "64",
            "--license", "Original test fixture; Apache-2.0", "--output", str(folder / "ui"),
            *extra], text=True, capture_output=True)

    def test_offline_subset_determinism_and_runtime_consumer(self):
        first, second = self.directory / "first", self.directory / "second"
        result = self.pack(first)
        self.assertEqual(result.returncode, 0, result.stderr)
        result = self.pack(second)
        self.assertEqual(result.returncode, 0, result.stderr)
        for suffix in [".fnt", ".atlas", ".png", ".font.json"]:
            self.assertEqual((first / f"ui{suffix}").read_bytes(), (second / f"ui{suffix}").read_bytes())
        report = json.loads((first / "ui.font.json").read_text())
        self.assertEqual(report["format"], "BMFont")
        self.assertEqual(report["missing"], [])
        self.assertFalse(report["license_verified_by_tool"])
        self.assertEqual(report["glyph_count"], 5)
        self.assertEqual(len(report["source_sha256"]), 64)
        subprocess.run([str(self.executable), str(first), "pipeline"], check=True)

    def test_real_host_preview_consumer(self):
        folder = self.directory / "preview"
        font = self.directory / "preview-source.ttf"
        fixture_font(font)
        text = self.directory / "preview-strings.txt"
        text.write_text("中文\nA?", encoding="utf-8")
        subprocess.run(["python3", str(ROOT / "tools/font_preview.py"),
            "--font", str(font), "--text-file", str(text),
            "--license", "Original test fixture; Apache-2.0", "--output", str(folder),
            "--width", "320", "--height", "240", "--size", "16"], check=True)
        metrics = json.loads((folder / "layout.json").read_text())
        self.assertEqual(metrics["glyphs"], 4)
        self.assertEqual(metrics["lines"], 2)
        self.assertEqual(metrics["missing_glyphs"], 0)
        from PIL import Image
        with Image.open(folder / "preview.png") as preview:
            self.assertEqual(preview.size, (320, 240))
            self.assertGreater(len(preview.getcolors(320 * 240)), 1)

    def test_missing_codepoints_fail_with_report(self):
        folder = self.directory / "missing"
        result = self.pack(folder, "--codepoint", "U+1F600")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("U+1F600", result.stderr)
        report = json.loads((folder / "ui.font.json").read_text())
        self.assertEqual(report["status"], "missing_glyphs")
        self.assertEqual(report["missing"], ["U+1F600"])
        self.assertFalse((folder / "ui.fnt").exists())
        result = self.pack(self.directory / "invalid", "--codepoint", "U+D800")
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
