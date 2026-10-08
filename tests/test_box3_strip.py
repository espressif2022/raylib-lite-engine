"""Independent RGB565 pixel-output oracle for the BOX-3 SPI strip presenter."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT / "examples/boards/esp32-s3-box-3"
LCD_WIDTH, LCD_HEIGHT = 320, 240

# The executable calls the same pure strip extractor as box3_video.c. It
# outputs the assembled full LCD frame; Python computes an independent oracle.
C_DRIVER = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "box3_strip.h"

static uint16_t pixel(uint32_t x, uint32_t y)
{
    return (uint16_t)((x * 19U) ^ (y * 53U) ^ ((x + y) << 6) ^ 0xa57dU);
}

int main(int argc, char **argv)
{
    if (argc != 5) return 2;
    uint16_t width = (uint16_t)strtoul(argv[1], NULL, 10);
    uint16_t height = (uint16_t)strtoul(argv[2], NULL, 10);
    uint16_t rows = (uint16_t)strtoul(argv[3], NULL, 10);
    bool swap = strtoul(argv[4], NULL, 10) != 0;
    if (!width || !height || !rows) return 3;

    box3_viewport_t viewport;
    if (!box3_viewport_init(&viewport, width, height, 320, 240)) return 4;

    uint16_t *frame = malloc((size_t)width * height * sizeof(uint16_t));
    uint16_t *strip = malloc((size_t)320 * rows * sizeof(uint16_t));
    if (!frame || !strip) return 5;

    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            frame[y * width + x] = pixel(x, y);

    for (uint32_t top = 0; top < 240; top += rows) {
        uint16_t count = (uint16_t)((240 - top) < rows ? (240 - top) : rows);
        box3_fill_strip_pixels(&viewport, frame, strip,
                               (uint16_t)top, count, swap);
        if (fwrite(strip, sizeof(uint16_t), (size_t)320 * count, stdout) !=
                (size_t)320 * count) return 6;
    }
    free(strip);
    free(frame);
    return 0;
}
"""


def oracle_frame(width: int, height: int, swap: bool) -> bytes:
    """Pixel-by-pixel expected nearest-neighbor mapping and letterbox fill."""
    if width * LCD_HEIGHT <= height * LCD_WIDTH:
        view_w = width * LCD_HEIGHT // height
        view_h = LCD_HEIGHT
    else:
        view_w = LCD_WIDTH
        view_h = height * LCD_WIDTH // width
    view_w = max(view_w, 1)
    view_h = max(view_h, 1)
    left = (LCD_WIDTH - view_w) // 2
    top = (LCD_HEIGHT - view_h) // 2

    expected = bytearray()
    for y in range(LCD_HEIGHT):
        for x in range(LCD_WIDTH):
            if not (left <= x < left + view_w and top <= y < top + view_h):
                color = 0
            else:
                source_x = (x - left) * width // view_w
                source_y = (y - top) * height // view_h
                color = ((source_x * 19) ^ (source_y * 53) ^
                         ((source_x + source_y) << 6) ^ 0xA57D) & 0xFFFF
                if swap:
                    color = (color >> 8) | ((color & 0xFF) << 8)
            expected.extend(color.to_bytes(2, sys.byteorder))
    return bytes(expected)


class Box3StripOracleTests(unittest.TestCase):
    def test_strip_pixels_match_independent_oracle(self):
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        if compiler is None:
            self.skipTest("C compiler not installed")
        presenter = (BOARD / "box3_video.c").read_text()
        self.assertIn("box3_fill_strip_pixels(&view,", presenter,
                      "Host oracle must exercise the actual production extractor")
        fixtures = (
            # Exact map, letterbox sides/top, down/upscale, odd aspect ratios.
            (480, 480, 8, False),
            (480, 480, 7, True),
            (320, 240, 8, False),
            (320, 240, 13, True),
            (240, 320, 8, True),
            (640, 480, 3, False),
            (333, 251, 7, True),
            (80, 120, 13, False),
            (1024, 192, 8, True),
            (192, 1024, 7, False),
            (1, 1, 9, True),
            (1, 3, 8, False),
            (3, 1, 7, True),
        )
        with tempfile.TemporaryDirectory(prefix="rle_box3_pixels_") as tmp:
            source = Path(tmp) / "strip_driver.c"
            binary = Path(tmp) / "strip_driver"
            source.write_text(C_DRIVER, encoding="utf-8")
            subprocess.run(
                [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-pedantic", "-I", str(BOARD), str(source), "-o", str(binary)],
                check=True, capture_output=True, text=True)
            for width, height, strip_rows, swap in fixtures:
                with self.subTest(width=width, height=height,
                                  rows=strip_rows, swap=swap):
                    output = subprocess.run(
                        [str(binary), str(width), str(height),
                         str(strip_rows), str(int(swap))],
                        check=True, capture_output=True).stdout
                    self.assertEqual(len(output), LCD_WIDTH * LCD_HEIGHT * 2)
                    self.assertEqual(output, oracle_frame(width, height, swap))


if __name__ == "__main__":
    unittest.main()
