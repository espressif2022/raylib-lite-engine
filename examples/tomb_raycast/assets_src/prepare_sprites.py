#!/usr/bin/env python3
"""Gold binary-alpha HUD rings for the tomb explorer stick and jump button."""
import os
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parent


def save_atomic(image, destination):
    if destination.exists():
        previous = Image.open(destination).convert("RGBA")
        if previous.size == image.size and previous.tobytes() == image.tobytes():
            print(f"unchanged {destination.name}")
            return
    temporary = destination.with_name(f"{destination.name}.tmp.{os.getpid()}.png")
    image.save(temporary, format="PNG")
    os.replace(temporary, destination)
    print(f"wrote {destination.name}")


def main():
    controls = Image.new("RGBA", (192, 96), (0, 0, 0, 0))
    draw = ImageDraw.Draw(controls)
    draw.ellipse((4, 4, 92, 92), outline=(212, 176, 88, 255), width=4)
    draw.ellipse((14, 14, 82, 82), outline=(96, 72, 32, 255), width=2)
    draw.ellipse((100, 4, 188, 92), outline=(236, 196, 96, 255), width=4)
    draw.polygon([(144, 22), (126, 48), (138, 48), (138, 72), (150, 72), (150, 48), (162, 48)],
                 fill=(248, 224, 160, 255))
    save_atomic(controls, root / "controls.png")


if __name__ == "__main__":
    main()
