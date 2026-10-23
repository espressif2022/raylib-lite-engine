#!/usr/bin/env python3
"""Build the five-pose Chibi Cyber Neko motorcycle sheet with a shared contact anchor."""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
RESAMPLING = getattr(Image, "Resampling", Image)
CELL = 240


def render_neko_pose(lean_deg: float) -> Image.Image:
    upright = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    d = ImageDraw.Draw(upright)

    outline = (20, 24, 38, 255)
    tire_dark = (32, 38, 52, 255)
    tire_mid = (52, 62, 82, 255)
    bike_main = (72, 232, 198, 255)
    bike_shade = (38, 168, 148, 255)
    jacket_main = (255, 96, 112, 255)
    jacket_shade = (208, 56, 78, 255)
    helmet_main = (242, 246, 255, 255)
    helmet_shade = (196, 210, 232, 255)
    ear_inner = (255, 138, 168, 255)
    scarf_c = (255, 214, 72, 255)
    stripe_c = (72, 235, 255, 255)

    cx = 120
    dy = 8  # Bottom of tire sits at y=232 (~97% of 240) to match view anchor

    # 1. Chubby Cartoon Rear Tire
    d.rounded_rectangle([cx - 32, 148 + dy, cx + 32, 224 + dy], radius=24, fill=outline)
    d.rounded_rectangle([cx - 26, 154 + dy, cx + 26, 218 + dy], radius=18, fill=tire_dark)
    d.rounded_rectangle([cx - 16, 158 + dy, cx + 16, 214 + dy], radius=10, fill=tire_mid)
    for ty in (168 + dy, 184 + dy, 200 + dy):
        d.rounded_rectangle([cx - 18, ty, cx + 18, ty + 5], radius=2, fill=outline)

    # 2. Twin Chubby Exhaust Mufflers
    for side in (-1, 1):
        ex = cx + side * 34
        d.ellipse([ex - 14, 170 + dy, ex + 14, 198 + dy], fill=outline)
        d.ellipse([ex - 10, 174 + dy, ex + 10, 194 + dy], fill=(168, 182, 204, 255))
        d.ellipse([ex - 5, 179 + dy, ex + 5, 189 + dy], fill=(255, 142, 68, 255))

    # 3. Rounded Rear Cowl & Big Pill Taillight
    d.rounded_rectangle([cx - 46, 124 + dy, cx + 46, 172 + dy], radius=22, fill=outline)
    d.rounded_rectangle([cx - 40, 130 + dy, cx + 40, 166 + dy], radius=18, fill=bike_shade)
    d.rounded_rectangle([cx - 36, 128 + dy, cx + 36, 156 + dy], radius=16, fill=bike_main)
    d.ellipse([cx - 26, 132 + dy, cx - 10, 140 + dy], fill=(245, 252, 255, 255))

    d.rounded_rectangle([cx - 24, 144 + dy, cx + 24, 164 + dy], radius=10, fill=outline)
    d.rounded_rectangle([cx - 20, 148 + dy, cx + 20, 160 + dy], radius=6, fill=(255, 58, 82, 255))
    d.rounded_rectangle([cx - 12, 150 + dy, cx + 12, 155 + dy], radius=3, fill=(255, 190, 200, 255))

    for side in (-1, 1):
        bx = cx + side * 44
        d.ellipse([bx - 9, 142 + dy, bx + 9, 160 + dy], fill=outline)
        d.ellipse([bx - 6, 145 + dy, bx + 6, 157 + dy], fill=(255, 206, 68, 255))

    # 4. Handlebars + Solidly Attached Round Lollipop Mirrors
    for side in (-1, 1):
        hx = cx + side * 50
        d.line([(cx + side * 28, 112 + dy), (hx, 104 + dy)], fill=outline, width=11)
        d.line([(cx + side * 28, 112 + dy), (hx, 104 + dy)], fill=(180, 198, 220, 255), width=5)
        mx, my = cx + side * 56, 92 + dy
        d.line([(hx, 104 + dy), (mx, my)], fill=outline, width=9)
        d.ellipse([mx - 12, my - 12, mx + 12, my + 12], fill=outline)
        d.ellipse([mx - 8, my - 8, mx + 8, my + 8], fill=stripe_c)
        d.ellipse([mx - 5, my - 5, mx + 1, my + 1], fill=(245, 252, 255, 255))

    # 5. Chibi Rider Body (Coral Hoodie + Little Boots)
    for side in (-1, 1):
        fx = cx + side * 38
        d.ellipse([fx - 13, 148 + dy, fx + 13, 168 + dy], fill=outline)
        d.ellipse([fx - 9, 152 + dy, fx + 9, 164 + dy], fill=(48, 56, 78, 255))
    d.rounded_rectangle([cx - 38, 88 + dy, cx + 38, 144 + dy], radius=20, fill=outline)
    d.rounded_rectangle([cx - 32, 94 + dy, cx + 32, 138 + dy], radius=16, fill=jacket_shade)
    d.rounded_rectangle([cx - 28, 92 + dy, cx + 28, 130 + dy], radius=14, fill=jacket_main)
    d.rounded_rectangle([cx - 14, 102 + dy, cx + 14, 128 + dy], radius=8, fill=outline)
    d.rounded_rectangle([cx - 10, 106 + dy, cx + 10, 124 + dy], radius=6, fill=scarf_c)

    # 6. Big Cute Cat-Ear Helmet
    head_shift = int(lean_deg * 0.22)
    hcx = cx + head_shift
    for side in (-1, 1):
        pts_out = [(hcx + side * 16, 42 + dy), (hcx + side * 44, 14 + dy), (hcx + side * 48, 54 + dy)]
        pts_in = [(hcx + side * 22, 42 + dy), (hcx + side * 40, 22 + dy), (hcx + side * 42, 50 + dy)]
        d.polygon(pts_out, fill=outline)
        d.polygon([(hcx + side * 18, 43 + dy), (hcx + side * 42, 18 + dy), (hcx + side * 45, 52 + dy)], fill=helmet_main)
        d.polygon(pts_in, fill=ear_inner)

    d.ellipse([hcx - 48, 28 + dy, hcx + 48, 106 + dy], fill=outline)
    d.ellipse([hcx - 42, 34 + dy, hcx + 42, 100 + dy], fill=helmet_shade)
    d.ellipse([hcx - 40, 32 + dy, hcx + 38, 92 + dy], fill=helmet_main)
    d.rounded_rectangle([hcx - 7, 33 + dy, hcx + 7, 96 + dy], radius=5, fill=stripe_c)
    d.ellipse([hcx - 28, 40 + dy, hcx - 10, 54 + dy], fill=(250, 253, 255, 255))
    d.ellipse([hcx - 8, 90 + dy, hcx + 8, 96 + dy], fill=outline)

    # 7. Fluttering Yellow Scarf
    scarf_shift = int(-lean_deg * 1.05)
    d.polygon([
        (cx - 18, 94 + dy), (cx + 22, 94 + dy),
        (cx + 38 + scarf_shift, 118 + dy), (cx + 18 + scarf_shift, 126 + dy),
        (cx - 6, 102 + dy),
    ], fill=outline)
    d.polygon([
        (cx - 14, 96 + dy), (cx + 18, 96 + dy),
        (cx + 33 + scarf_shift, 116 + dy), (cx + 18 + scarf_shift, 122 + dy),
        (cx - 4, 100 + dy),
    ], fill=scarf_c)

    if abs(lean_deg) > 0.5:
        upright = upright.rotate(-lean_deg, resample=RESAMPLING.BICUBIC, center=(120, 220))

    px = upright.load()
    for y in range(CELL):
        for x in range(CELL):
            r, g, b, a = px[x, y]
            if a < 128:
                px[x, y] = (0, 0, 0, 0)
            else:
                px[x, y] = (r, g, b, 255)
    # Rotation changes the lowest opaque pixel. Keep all poses on the same
    # ground plane as the upright tire (inclusive y=232).
    bounds = upright.getchannel("A").getbbox()
    if bounds and bounds[3] != 233:
        aligned = Image.new("RGBA", upright.size, (0, 0, 0, 0))
        aligned.paste(upright, (0, 233 - bounds[3]))
        upright = aligned
    return upright


def main() -> None:
    sheet = Image.new("RGBA", (CELL * 5, CELL), (0, 0, 0, 0))
    for index, angle in enumerate((-22.0, -11.0, 0.0, 11.0, 22.0)):
        sheet.paste(render_neko_pose(angle), (index * CELL, 0))
    out = ROOT / "motorcycle_poses_v2_source.png"
    tmp = out.with_suffix(".tmp.png")
    sheet.save(tmp, format="PNG")
    tmp.replace(out)


if __name__ == "__main__":
    main()
