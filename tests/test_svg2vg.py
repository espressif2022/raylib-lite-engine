# SPDX-License-Identifier: Apache-2.0
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import svg2vg  # noqa: E402

RUNTIME = ROOT / "examples" / "vector_cat" / "main"
RENDERER = ROOT / "src" / "renderer"
ENGINE_INCLUDE = ROOT / "include" / "raylib_lite"

RIG_SVG = """\
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <defs><linearGradient id="g"><stop offset="0" stop-color="#00ff00"/></linearGradient></defs>
  <g id="body">
    <rect x="20" y="40" width="60" height="30" fill="#ff0000"/>
    <circle id="body-pivot" cx="50" cy="55" r="1"/>
  </g>
  <g id="arm@body" transform="translate(10 0)">
    <rect x="60" y="45" width="30" height="10" fill="url(#g)"/>
    <circle id="pivot" cx="60" cy="50" r="1"/>
  </g>
</svg>
"""


def convert(text: str):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "in.svg"
        path.write_text(text)
        return svg2vg.load_svg(path)


class PathParsing(unittest.TestCase):
    def test_relative_and_shorthand_commands(self):
        cmds = svg2vg.parse_path("m10 10 h5 v5 l-5 0 z")
        self.assertEqual(cmds, [("M", 10, 10), ("L", 15, 10), ("L", 15, 15), ("L", 10, 15),
                                ("Z",)])

    def test_implicit_lineto_after_move(self):
        cmds = svg2vg.parse_path("M0 0 10 0 10 10")
        self.assertEqual([c[0] for c in cmds], ["M", "L", "L"])

    def test_smooth_cubic_reflects_control_point(self):
        cmds = svg2vg.parse_path("M0 0 C0 10 10 10 10 0 S20 -10 20 0")
        self.assertEqual(cmds[2][1:3], (10, -10))

    def test_quadratic_becomes_cubic(self):
        cmds = svg2vg.parse_path("M0 0 Q10 10 20 0")
        self.assertEqual(cmds[1][0], "C")
        self.assertAlmostEqual(cmds[1][1], 20 / 3)
        self.assertAlmostEqual(cmds[1][2], 20 / 3)

    def test_arc_ends_on_target_and_stays_on_circle(self):
        cmds = svg2vg.parse_path("M10 0 A10 10 0 0 1 -10 0")
        end = cmds[-1]
        self.assertAlmostEqual(end[5], -10, places=5)
        self.assertAlmostEqual(end[6], 0, places=5)
        self.assertGreaterEqual(len(cmds) - 1, 2)

    def test_transform_composition(self):
        m = svg2vg.parse_transform("translate(10 20) scale(2)")
        self.assertEqual(svg2vg.mat_apply(m, 1, 1), (12, 22))
        m = svg2vg.parse_transform("rotate(90 5 5)")
        x, y = svg2vg.mat_apply(m, 10, 5)
        self.assertAlmostEqual(x, 5)
        self.assertAlmostEqual(y, 10)


class LayerModel(unittest.TestCase):
    def test_parts_parents_pivots_and_gradient_fallback(self):
        parts, warnings, size = convert(RIG_SVG)
        self.assertEqual(size, (100, 100))
        self.assertEqual([(p.name, p.parent) for p in parts], [("body", None), ("arm", "body")])
        self.assertEqual(parts[0].pivot, (50, 55))
        self.assertEqual(parts[1].pivot, (70, 50))
        self.assertEqual(parts[1].shapes[0].color, (0, 255, 0))
        self.assertTrue(any("gradient" in w for w in warnings))

    def test_rejects_masks_unknown_parents_and_loose_shapes(self):
        bad = [
            '<svg xmlns="http://www.w3.org/2000/svg"><g id="a"><mask id="m"/></g></svg>',
            '<svg xmlns="http://www.w3.org/2000/svg"><g id="a@nope"><rect width="1" height="1"/></g></svg>',
            '<svg xmlns="http://www.w3.org/2000/svg"><rect width="1" height="1"/><g id="a"/></svg>',
        ]
        for text in bad:
            with self.subTest(text=text), self.assertRaises(svg2vg.SvgError):
                convert(text)

    def test_generated_names_match_parts(self):
        parts, _, size = convert(RIG_SVG)
        header, source = svg2vg.emit(parts, "rig", size)
        self.assertIn("#define RIG_BODY 0", header)
        self.assertIn("#define RIG_ARM 1", header)
        self.assertIn("#define RIG_PART_COUNT 2", header)
        self.assertIn('{"arm", 0, 70.00f, 50.00f', source)


RENDER_MAIN = r"""
#include <math.h>
#include <stdio.h>
#include "rig.h"
static uint16_t fb[100 * 100];
int main(int argc, char **argv)
{
    (void)argv;
    vg_bone_t bones[RIG_PART_COUNT];
    for (int i = 0; i < RIG_PART_COUNT; ++i) bones[i] = vg_bone_rest();
    if (argc > 1) bones[RIG_BODY].rotation = 3.14159265f / 2;
    vg_mat_t mats[RIG_PART_COUNT];
    vg_asset_pose(&rig, bones, vg_identity(), mats);
    vg_begin(fb, 100, 100, 100);
    uint8_t visible[RIG_PART_COUNT] = {1, 1};
    if (argc > 2) visible[RIG_ARM] = 0;
    vg_asset_draw_range(&rig, mats, visible, 0, RIG_PART_COUNT);
    fwrite(fb, sizeof(fb), 1, stdout);
    return 0;
}
"""


@unittest.skipUnless(shutil.which("cc"), "needs a host C compiler")
class RenderedPose(unittest.TestCase):
    def render(self, rotate: bool, hide_arm: bool = False) -> bytes:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / "in.svg").write_text(RIG_SVG)
            self.assertEqual(svg2vg.main([str(out / "in.svg"), "--name", "rig",
                                          "--out-dir", str(out)]), 0)
            (out / "main.c").write_text(RENDER_MAIN)
            exe = out / "render"
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(out),
                            "-I", str(RUNTIME), "-I", str(ENGINE_INCLUDE), str(out / "main.c"), str(out / "rig.c"),
                            str(RUNTIME / "vg_asset.c"), str(RUNTIME / "vg_raster.c"), str(RENDERER / "raylib_lite_rgb565.c"),
                            "-lm", "-o", str(exe)], check=True)
            return subprocess.run([str(exe)] + (["rotate"] if rotate or hide_arm else [])
                                  + (["hide"] if hide_arm else []),
                                  check=True, capture_output=True).stdout

    @staticmethod
    def pixel(frame: bytes, x: int, y: int) -> int:
        i = (y * 100 + x) * 2
        return frame[i] | frame[i + 1] << 8

    def test_rest_pose_matches_artwork(self):
        frame = self.render(False)
        self.assertEqual(self.pixel(frame, 30, 50), 0xF800)   # body
        self.assertEqual(self.pixel(frame, 85, 50), 0x07E0)   # arm, shifted by its group
        self.assertEqual(self.pixel(frame, 10, 10), 0)

    def test_child_follows_parent_rotation(self):
        frame = self.render(True)
        # A quarter turn about (50, 55) moves the arm from the right to below.
        self.assertEqual(self.pixel(frame, 50, 88), 0x07E0)
        self.assertEqual(self.pixel(frame, 85, 50), 0)
        self.assertEqual(self.pixel(frame, 50, 40), 0xF800)

    def test_hidden_layer_is_skipped(self):
        frame = self.render(True, hide_arm=True)
        self.assertEqual(self.pixel(frame, 50, 88), 0)
        self.assertEqual(self.pixel(frame, 50, 40), 0xF800)


if __name__ == "__main__":
    unittest.main()
