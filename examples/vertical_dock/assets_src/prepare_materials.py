"""Resize the authored 2x2 sheets for the RGB565 game atlases."""
from pathlib import Path

from PIL import Image, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parent
def prepare(name: str) -> None:
    source = ROOT / f"{name}_source.png"
    destination = ROOT / f"{name}.png"
    with Image.open(source) as image:
        image = image.convert("RGB")
        width, height = image.size
        resampling = getattr(Image, "Resampling", Image)
        output = Image.new("RGB", (512, 512))
        base_colors = ((78, 88, 91), (69, 94, 98),
                       (101, 87, 70), (117, 107, 74))
        blend_amounts = (.20, .10, .14, .37)
        for row in range(2):
            for column in range(2):
                box = (column * width // 2, row * height // 2,
                       (column + 1) * width // 2, (row + 1) * height // 2)
                tile = image.crop(box).resize((256, 256), resampling.LANCZOS)
                if name == "materials":
                    index = row * 2 + column
                    tile = ImageEnhance.Contrast(tile).enhance(.86)
                    tile = tile.filter(ImageFilter.GaussianBlur(.35))
                    tile = Image.blend(tile,
                        Image.new("RGB", tile.size, base_colors[index]),
                        blend_amounts[index])
                output.paste(tile, (column * 256, row * 256))
        if not destination.exists() or Image.open(destination).convert("RGB").tobytes() != output.tobytes():
            output.save(destination, optimize=True)


for sheet in ("materials", "details"):
    prepare(sheet)
