#!/usr/bin/env python3
"""Build the five-pose motorcycle source sheet with a shared contact anchor."""
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent
CELL = 724
BOTTOM = 704
MAX_SIZE = 684


def normalized(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    alpha = image.getchannel("A")
    bounds = alpha.getbbox()
    if bounds is None:
        raise ValueError("motorcycle pose has no visible pixels")
    crop = image.crop(bounds)
    scale = min(MAX_SIZE / crop.width, MAX_SIZE / crop.height)
    size = (max(1, round(crop.width * scale)), max(1, round(crop.height * scale)))
    return crop.resize(size, Image.LANCZOS)


def paste_centered(sheet: Image.Image, pose: Image.Image, index: int) -> None:
    x = index * CELL + (CELL - pose.width) // 2
    y = BOTTOM - pose.height
    sheet.alpha_composite(pose, (x, y))


original = Image.open(ROOT / "motorcycle_poses_source.png").convert("RGBA")
if original.size != (CELL * 3, CELL):
    raise ValueError(f"unexpected original pose sheet size: {original.size}")

# The original frame names describe the visible side. From the rear gameplay
# camera the third cell reads as a left turn and the first as a right turn.
heavy_right = original.crop((0, 0, CELL, CELL))
straight = original.crop((CELL, 0, CELL * 2, CELL))
heavy_left = original.crop((CELL * 2, 0, CELL * 3, CELL))
light_left = Image.open(ROOT / "motorcycle_light_left_generated.png")
light_right = Image.open(ROOT / "motorcycle_light_right_generated.png")

sheet = Image.new("RGBA", (CELL * 5, CELL), (0, 0, 0, 0))
for index, source in enumerate((heavy_left, light_left, straight,
                                light_right, heavy_right)):
    paste_centered(sheet, normalized(source), index)
sheet.save(ROOT / "motorcycle_poses_v2_source.png")
