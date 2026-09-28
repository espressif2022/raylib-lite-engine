#!/usr/bin/env python3
"""Normalize generated rally art into a deterministic 4x3 atlas source."""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parent
CELL = 128
CROPS = [
    (24, 12, 700, 370), (720, 10, 1254, 370),
    (20, 350, 700, 700), (700, 350, 1254, 700),
    (20, 680, 650, 1010), (680, 680, 1254, 1010),
    (0, 990, 320, 1254), (290, 990, 630, 1254),
    (600, 990, 940, 1254), (900, 990, 1254, 1254),
]

source = Image.open(ROOT / "rally_sprites.png").convert("RGBA")
sheet = Image.new("RGBA", (CELL * 4, CELL * 3), (0, 0, 0, 0))
for index, box in enumerate(CROPS):
    item = source.crop(box)
    alpha_box = item.getchannel("A").getbbox()
    if alpha_box:
        item = item.crop(alpha_box)
    scale = min((CELL - 8) / item.width, (CELL - 8) / item.height)
    size = (max(1, int(item.width * scale)), max(1, int(item.height * scale)))
    item = item.resize(size, Image.Resampling.LANCZOS)
    x = (index % 4) * CELL + (CELL - size[0]) // 2
    y = (index // 4) * CELL + (CELL - size[1]) // 2
    sheet.alpha_composite(item, (x, y))

# Roadside props are authored as independent transparent renders rather than
# baked into a backdrop.  They occupy the last atlas row and are projected by
# the runtime from deterministic track-segment anchors.
for index, filename in enumerate(("roadside_rock_source.png",
                                  "roadside_beacon_source.png"), start=10):
    item = Image.open(ROOT / filename).convert("RGBA")
    alpha_box = item.getchannel("A").getbbox()
    if alpha_box:
        item = item.crop(alpha_box)
    scale = min((CELL - 8) / item.width, (CELL - 8) / item.height)
    size = (max(1, int(item.width * scale)), max(1, int(item.height * scale)))
    item = item.resize(size, Image.Resampling.LANCZOS)
    x = (index % 4) * CELL + (CELL - size[0]) // 2
    y = (index // 4) * CELL + (CELL - size[1]) // 2
    sheet.alpha_composite(item, (x, y))
sheet.save(ROOT / "rally_atlas_source.png")
