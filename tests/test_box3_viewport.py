"""Portable geometry tests for the BOX-3 logical game viewport."""
import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
VIEWPORT = ROOT / "examples/boards/esp32-s3-box-3"


class Box3ViewportTests(unittest.TestCase):
    def test_viewport_and_touch_coordinates(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("C compiler not installed")
        program = r"""
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "box3_viewport.h"

static void check(uint16_t lw, uint16_t lh, uint16_t vw, uint16_t vh,
                  uint16_t vx, uint16_t vy) {
    box3_viewport_t v;
    assert(box3_viewport_init(&v, lw, lh, 320, 240));
    assert(v.view_width == vw && v.view_height == vh);
    assert(v.view_x == vx && v.view_y == vy);
    int32_t x = vx, y = vy;
    assert(box3_viewport_map_touch(&v, &x, &y) && x == 0 && y == 0);
    x = vx + vw - 1; y = vy + vh - 1;
    assert(box3_viewport_map_touch(&v, &x, &y));
    assert(x < lw && y < lh);
    if (vx) {
        x = vx - 1; y = vy + vh/2;
        assert(!box3_viewport_map_touch(&v, &x, &y));
        assert(x == vx - 1);
        x = vx + vw; y = vy + vh/2;
        assert(!box3_viewport_map_touch(&v, &x, &y));
    }
    if (vy) {
        x = vx + vw/2; y = vy - 1;
        assert(!box3_viewport_map_touch(&v, &x, &y));
    }
}
int main(void) {
    check(480, 480, 240, 240, 40, 0);
    check(320, 240, 320, 240, 0, 0);
    check(240, 320, 180, 240, 70, 0);
    check(640, 480, 320, 240, 0, 0);
    check(1, 1, 240, 240, 40, 0);
    box3_viewport_t v;
    assert(!box3_viewport_init(&v, 0, 480, 320, 240));
    assert(!box3_viewport_init(&v, 480, 0, 320, 240));
    assert(!box3_viewport_init(NULL, 480, 480, 320, 240));
    int32_t x = 279, y = 239;
    assert(box3_viewport_init(&v, 480, 480, 320, 240));
    assert(box3_viewport_map_touch(&v, &x, &y));
    assert(x == 478 && y == 478);
    return 0;
}
"""
        with tempfile.TemporaryDirectory(prefix="rle_box3_viewport_") as tmp:
            source = pathlib.Path(tmp) / "viewport_test.c"
            output = pathlib.Path(tmp) / "viewport_test"
            source.write_text(program, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-pedantic", "-I", str(VIEWPORT), str(source), "-o", str(output)],
                check=True, capture_output=True, text=True)
            subprocess.run([str(output)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
