#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate the example's own small image, font and PCM fixtures."""
from array import array
from math import pi, sin
from pathlib import Path
import sys
import wave

from PIL import Image, ImageDraw


def write_tone(path, hz, seconds, amplitude, decay=False):
    rate = 24000
    length = int(rate * seconds)
    samples = array("h", (int(amplitude * sin(2 * pi * hz * i / rate)
                               * (1 - i / length if decay else 1))
                          for i in range(length)))
    if sys.byteorder != "little":
        samples.byteswap()
    with wave.open(str(path), "wb") as stream:
        stream.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        stream.writeframes(samples.tobytes())


def prepare(destination):
    sprite = Image.new("RGBA", (11, 11))
    ImageDraw.Draw(sprite).polygon(
        [(5, 0), (7, 3), (10, 4), (8, 6), (9, 10), (5, 8),
         (1, 10), (2, 6), (0, 4), (3, 3)], fill=(255, 223, 95, 255))
    sprite.save(destination / "sprite.png")

    glyphs = Image.new("RGBA", (9, 9))
    draw = ImageDraw.Draw(glyphs)
    draw.rectangle((1, 2, 7, 6), outline="white")
    draw.line((4, 0, 4, 8), fill="white")
    glyphs.save(destination / "glyphs.png")
    (destination / "glyphs.fnt").write_text(
        'info face="Native2D" size=9\n'
        'common lineHeight=9 base=8 scaleW=9 scaleH=9 pages=1 packed=0\n'
        'page id=0 file="glyphs.png"\n'
        'chars count=1\n'
        'char id=20013 x=0 y=0 width=9 height=9 xoffset=0 yoffset=0 xadvance=10 page=0 chnl=15\n',
        encoding="ascii",
    )
    write_tone(destination / "catch.wav", 740, 0.12, 11000, decay=True)
    write_tone(destination / "music.wav", 220, 0.5, 2800)


if __name__ == "__main__":
    prepare(Path(__file__).resolve().parent)
