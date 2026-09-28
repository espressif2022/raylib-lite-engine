#!/usr/bin/env python3
"""Resize the approved environment plate into the device-native atlas cell."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
image = Image.open(root / "track_vehicle_first_source.png").convert("RGB")
image = image.resize((480, 480), Image.Resampling.LANCZOS)
image.save(root / "track_canyon.png", optimize=True)
