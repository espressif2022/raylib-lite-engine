# SPDX-License-Identifier: Apache-2.0
"""MTX2 decoder oracle plus an informational sampler benchmark.

The oracle synthesises blocks directly rather than going through
tools/mtx2_codec.py, so it pins the *format contract* and covers both palette
modes and degenerate endpoints. Encoder quality is measured separately.
"""
import os
from pathlib import Path
import random
import shlex
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

MTX2_HEADER = struct.Struct("<4sHHHHHHI")
BLOCK_W, BLOCK_H = 64, 64          # oracle texture, in blocks
# Benchmark textures are generated inside the C harness; only the oracle needs Python.


def palette565(c0: int, c1: int) -> tuple[list[int], bool]:
    """Reference palette, independent of the encoder implementation."""
    r0, g0, b0 = (c0 >> 11) & 31, (c0 >> 5) & 63, c0 & 31
    r1, g1, b1 = (c1 >> 11) & 31, (c1 >> 5) & 63, c1 & 31
    punch = c0 <= c1
    if punch:
        p2 = (((r0 + r1) // 2) << 11) | (((g0 + g1) // 2) << 5) | ((b0 + b1) // 2)
        p3 = 0
    else:
        p2 = ((((2 * r0 + r1) // 3) << 11) | (((2 * g0 + g1) // 3) << 5)
              | ((2 * b0 + b1) // 3))
        p3 = ((((r0 + 2 * r1) // 3) << 11) | (((g0 + 2 * g1) // 3) << 5)
              | ((b0 + 2 * b1) // 3))
    return [c0, c1, p2, p3], punch


def shade565(pixel: int, light: int) -> int:
    """Mirror of mosaico_shade565 for the LUT-aligned light levels used here."""
    if light >= 256:
        return pixel
    r, g, b = (pixel >> 11) & 31, (pixel >> 5) & 63, pixel & 31
    return ((r * light >> 8) << 11) | ((g * light >> 8) << 5) | (b * light >> 8)


def make_blocks(rng: random.Random, count: int, punch_ratio: float) -> bytes:
    """punch_ratio == 0 upholds the opaque invariant: c0 > c1 in every block."""
    out = bytearray()
    for i in range(count):
        a, b = rng.randrange(0x10000), rng.randrange(0x10000)
        if rng.random() < punch_ratio:
            c0, c1 = min(a, b), max(a, b)
            if i % 37 == 0:  # degenerate: identical endpoints, still punch
                c1 = c0
        else:
            c0, c1 = max(a, b), min(a, b)
            if c0 == c1:  # an opaque block may never collapse to punch mode
                c0 = min(c1 + 1, 0xFFFF)
                c1 = c0 - 1
        out += struct.pack("<HHI", c0, c1, rng.randrange(1 << 32))
    return bytes(out)


def decode_reference(blocks: bytes, bw: int, bh: int, light: int) -> list[int]:
    """Decode every texel to RGB565; transparent texels decode to -1."""
    width, height = bw * 4, bh * 4
    out = [0] * (width * height)
    for by in range(bh):
        for bx in range(bw):
            off = (by * bw + bx) * 8
            c0, c1, bits = struct.unpack_from("<HHI", blocks, off)
            pal, punch = palette565(c0, c1)
            pal = [shade565(p, light) for p in pal]
            for ty in range(4):
                for tx in range(4):
                    sel = (bits >> (ty * 8 + 2 * tx)) & 3
                    value = -1 if (punch and sel == 3) else pal[sel]
                    out[(by * 4 + ty) * width + bx * 4 + tx] = value
    return out


FLAG_OPAQUE = 1 << 1


def build_blob(blocks: bytes, bw: int, bh: int, flags: int = 0) -> bytes:
    return MTX2_HEADER.pack(b"MTX2", bw * 4, bh * 4, 0, flags, 1, 0, len(blocks)) + blocks


class Mtx2Tests(unittest.TestCase):
    def test_decoder_matches_reference_and_benchmarks(self):
        rng = random.Random(20260922)
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)

            # Two oracles: a punch-through texture for the transparency path and
            # an all-opaque one so the unscaled 1:1 fast path is covered too.
            for name, ratio, flags in (("punch", 0.4, 0), ("opaque", 0.0, FLAG_OPAQUE)):
                blocks = make_blocks(rng, BLOCK_W * BLOCK_H, punch_ratio=ratio)
                (temp / f"{name}.mtx2").write_bytes(
                    build_blob(blocks, BLOCK_W, BLOCK_H, flags))
                expected = bytearray()
                for light in (256, 192):
                    for value in decode_reference(blocks, BLOCK_W, BLOCK_H, light):
                        expected += struct.pack("<i", value)
                (temp / f"{name}.expect").write_bytes(bytes(expected))

            binary = temp / "mtx2"
            command = [os.environ.get("CC", "cc"), "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
            command += shlex.split(os.environ.get("CFLAGS", ""))
            command += [
                str(ROOT / "tests/test_mtx2.c"),
                str(ROOT / "components/mosaico_game_2d/mosaico_mtx2.c"),
                str(ROOT / "components/mosaico_game_2d/mosaico_rgb565.c"),
            ]
            command += ["-I", str(ROOT / "components/mosaico_game_2d/include")]
            command += ["-lm", "-o", str(binary)]
            subprocess.run(command, check=True)
            subprocess.run([str(binary), str(temp), str(BLOCK_W)], check=True)


if __name__ == "__main__":
    unittest.main()
