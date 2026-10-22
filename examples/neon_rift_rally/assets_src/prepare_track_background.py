#!/usr/bin/env python3
"""Normalize three authored panoramas for the compact RGB565 sky atlas."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
source = Image.open(root / "rally_panoramas_source.png").convert("RGB")
cell = 224
sheet = Image.new("RGB", (cell, cell * 3))
for index in range(3):
    panel = source.crop((0, index * source.height // 3,
                         source.width, (index + 1) * source.height // 3))
    # The view expands the square cell to a 480x224 panorama.
    panel = panel.resize((cell, cell), getattr(Image, "Resampling", Image).LANCZOS)
    sheet.paste(panel, (0, index * cell))
sheet.save(root / "rally_panoramas.png", optimize=True)
