#!/usr/bin/env python3
"""Validate the five-pose motorcycle source and atlas contract."""
import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1] / "assets_src"
config = json.loads((ROOT / "motorcycle_poses.json").read_text())
expected = [
    "motorcycle_heavy_left", "motorcycle_light_left", "motorcycle_straight",
    "motorcycle_light_right", "motorcycle_heavy_right",
]
assert [frame["name"] for frame in config["frames"]] == expected
assert config["columns"] == 5 and config["rows"] == 1
image = Image.open(ROOT / config["image"]).convert("RGBA")
cell = image.width // config["columns"]
assert image.size == (cell * 5, cell)

bottoms = []
for index, name in enumerate(expected):
    frame = image.crop((index * cell, 0, (index + 1) * cell, cell))
    alpha = frame.getchannel("A")
    bbox = alpha.getbbox()
    assert bbox is not None, name
    assert alpha.getpixel((0, 0)) == 0 and alpha.getpixel((cell - 1, 0)) == 0, name
    bottoms.append(bbox[3])
assert max(bottoms) - min(bottoms) <= 2, bottoms
print("neon_rift_rally five-pose names, transparency and rear contact alignment passed")
