#!/usr/bin/env python3
"""Reduce the generated nebula vista to one opaque RGB565 atlas cell.

The play field above the ground strip is 480x390; the view draws the top
240x195 of the cell at exactly 2x, so the cell keeps clean pixel clusters.
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
CELL = 240
ROWS = 195

source = Image.open(ROOT / "shooter_nebula_source.jpg").convert("RGB")
field = source.crop((0, 0, source.width, source.width * ROWS // CELL))
field = field.resize((CELL, ROWS), Image.LANCZOS)
cell = Image.new("RGB", (CELL, CELL))
cell.paste(field, (0, 0))
cell.save(ROOT / "shooter_nebula.png", optimize=True)
