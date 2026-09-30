"""Prepare the original sewer materials sheet for the 512x512 RGB565 atlas."""
from pathlib import Path
from PIL import Image, ImageEnhance

ROOT = Path(__file__).resolve().parent
source = ROOT / "materials_source.png"
target = ROOT / "materials.png"
with Image.open(source) as image:
    image = image.convert("RGB")
    width, height = image.size
    result = Image.new("RGB", (512, 512))
    resampling = getattr(Image, "Resampling", Image)
    for row in range(2):
        for column in range(2):
            tile = image.crop((column * width // 2, row * height // 2,
                               (column + 1) * width // 2, (row + 1) * height // 2))
            tile = tile.resize((256, 256), resampling.LANCZOS)
            tile = ImageEnhance.Contrast(tile).enhance(0.84)
            result.paste(tile, (column * 256, row * 256))
    result.save(target, optimize=True)
