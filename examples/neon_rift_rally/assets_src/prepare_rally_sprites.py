#!/usr/bin/env python3
"""Normalize authored roadside objects; keep their ground contact aligned."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
source = Image.open(root / "rally_props_source.png").convert("RGBA")
w, h = source.size
cell = 128
# Chevron, canyon rock, city barrier, snow rock. The snow silhouette crosses
# the source sheet midpoint, so use explicit crops rather than a grid split.
boxes = [(int(w * .58), h // 2, w, h),
         (w // 2, 0, w, h // 2), (0, 0, w // 2, h // 2),
         (0, h // 2, int(w * .55), h)]
sheet = Image.new("RGBA", (cell * 2, cell * 2))
for index, box in enumerate(boxes):
    item = source.crop(box)
    # Binary-alpha sprites should be bounded by pixels that survive packing;
    # faint generation/resize fringes must not lift their contact point.
    item.putalpha(item.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
    bounds = item.getchannel("A").getbbox()
    if not bounds:
        raise ValueError(f"empty roadside sprite {index}")
    item = item.crop(bounds)
    scale = min((cell - 8) / item.width, (cell - 8) / item.height)
    size = (max(1, round(item.width * scale)), max(1, round(item.height * scale)))
    item = item.resize(size, getattr(Image, "Resampling", Image).LANCZOS)
    item.putalpha(item.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
    item = item.crop(item.getchannel("A").getbbox())
    x = index % 2 * cell + (cell - item.width) // 2
    y = index // 2 * cell + cell - 2 - item.height
    sheet.alpha_composite(item, (x, y))
sheet.save(root / "rally_atlas_source.png", optimize=True)
