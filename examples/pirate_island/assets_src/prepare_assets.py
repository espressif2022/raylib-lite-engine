#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate hand-drawn storybook background, sprite atlas, and Chinese font for Pirate Island."""
import json
import math
import os
import random
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
RESAMPLING = getattr(Image, "Resampling", Image)
INK = (38, 42, 46, 255)


def save_atomic(image: Image.Image, destination: Path) -> None:
    tmp = destination.with_name(f"{destination.name}.tmp.{os.getpid()}.png")
    image.save(tmp, format="PNG")
    os.replace(tmp, destination)


def wobbly_line(d: ImageDraw.ImageDraw, p0: tuple[float, float], p1: tuple[float, float],
                fill=INK, width: int = 4, seed: int = 1) -> None:
    rng = random.Random(seed)
    x0, y0 = p0
    x1, y1 = p1
    dist = math.hypot(x1 - x0, y1 - y0)
    steps = max(2, int(dist // 14))
    pts = [(x0, y0)]
    for i in range(1, steps):
        t = i / steps
        pts.append((x0 + (x1 - x0) * t + rng.uniform(-1.5, 1.5),
                    y0 + (y1 - y0) * t + rng.uniform(-1.5, 1.5)))
    pts.append((x1, y1))
    d.line(pts, fill=fill, width=width, joint="curve")


def wobbly_poly(d: ImageDraw.ImageDraw, pts: list[tuple[float, float]],
                fill, outline=INK, width: int = 4, seed: int = 1) -> None:
    d.polygon(pts, fill=fill)
    if outline:
        for i in range(len(pts)):
            wobbly_line(d, pts[i], pts[(i + 1) % len(pts)], fill=outline, width=width, seed=seed + i * 7)


def build_backdrop() -> None:
    im = Image.new("RGB", (240, 240), (212, 234, 244))
    px = im.load()
    for y in range(240):
        t = y / 239.0
        r = int(198 + 34 * t)
        g = int(226 + 18 * t)
        b = int(242 + 6 * t)
        for x in range(240):
            tex = int(math.sin(x * 0.31) * math.cos(y * 0.29) * 3.0) + ((x * 7 + y * 13) % 5 - 2)
            px[x, y] = (max(0, min(255, r + tex)),
                        max(0, min(255, g + tex)),
                        max(0, min(255, b + tex)))
    save_atomic(im, ROOT / "story_bg_source.png")
    cfg = {
        "image": "story_bg_source.png",
        "columns": 1,
        "rows": 1,
        "output_cell": 240,
        "output_columns": 1,
        "alpha_mode": "opaque",
        "frames": [{"name": "story_paper_bg", "source_index": 0}],
    }
    (ROOT / "story_bg.json").write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")


def draw_pirate_cell(d: ImageDraw.ImageDraw, ox: int, oy: int, S: int,
                     hat_color, shirt_stripe, prop: str = "sword", decoy: bool = False) -> None:
    cx, cy = ox + 32 * S, oy + 58 * S
    d.line([(cx - 5 * S, cy - 14 * S), (cx - 7 * S, cy)], fill=INK, width=3 * S)
    d.line([(cx + 5 * S, cy - 14 * S), (cx + 7 * S, cy)], fill=INK, width=3 * S)
    d.rounded_rectangle([cx - 11 * S, cy - 32 * S, cx + 11 * S, cy - 12 * S], radius=5 * S, fill=INK)
    d.rounded_rectangle([cx - 9 * S, cy - 30 * S, cx + 9 * S, cy - 14 * S], radius=4 * S, fill=(252, 248, 240, 255))
    for sy in (-27, -22, -17):
        d.line([(cx - 8 * S, cy + sy * S), (cx + 8 * S, cy + sy * S)], fill=shirt_stripe, width=2 * S)
    if prop == "sword":
        d.line([(cx + 10 * S, cy - 22 * S), (cx + 23 * S, cy - 32 * S)], fill=INK, width=5 * S)
        d.line([(cx + 10 * S, cy - 22 * S), (cx + 23 * S, cy - 32 * S)], fill=(210, 220, 232, 255), width=3 * S)
    elif prop == "bomb":
        d.line([(cx - 10 * S, cy - 22 * S), (cx - 18 * S, cy - 18 * S)], fill=INK, width=3 * S)
        d.ellipse([cx + 9 * S, cy - 32 * S, cx + 25 * S, cy - 16 * S], fill=INK)
        d.ellipse([cx + 11 * S, cy - 30 * S, cx + 23 * S, cy - 18 * S], fill=(64, 68, 76, 255))
        d.ellipse([cx + 18 * S, cy - 36 * S, cx + 24 * S, cy - 30 * S], fill=(255, 196, 64, 255))
    else:
        d.line([(cx - 10 * S, cy - 22 * S), (cx - 18 * S, cy - 18 * S)], fill=INK, width=3 * S)
        d.ellipse([cx + 10 * S, cy - 28 * S, cx + 20 * S, cy - 18 * S], fill=INK)
        d.ellipse([cx + 12 * S, cy - 26 * S, cx + 18 * S, cy - 20 * S], fill=(252, 212, 78, 255))

    d.ellipse([cx - 12 * S, cy - 50 * S, cx + 12 * S, cy - 26 * S], fill=INK)
    d.ellipse([cx - 10 * S, cy - 48 * S, cx + 10 * S, cy - 28 * S], fill=(252, 246, 236, 255))
    d.line([(cx - 9 * S, cy - 42 * S), (cx + 9 * S, cy - 35 * S)], fill=INK, width=2 * S)
    d.ellipse([cx - 6 * S, cy - 41 * S, cx - 1 * S, cy - 36 * S], fill=INK)
    d.ellipse([cx + 3 * S, cy - 40 * S, cx + 6 * S, cy - 37 * S], fill=INK)
    if not decoy:
        wobbly_poly(d, [(cx - 19 * S, cy - 45 * S), (cx, cy - 60 * S), (cx + 19 * S, cy - 45 * S)],
                    fill=hat_color, outline=INK, width=3 * S, seed=ox + oy)
        d.ellipse([cx - 3 * S, cy - 52 * S, cx + 3 * S, cy - 46 * S], fill=(252, 248, 240, 255))
    else:
        d.ellipse([cx - 19 * S, cy - 50 * S, cx + 19 * S, cy - 42 * S], fill=INK)
        d.ellipse([cx - 17 * S, cy - 48 * S, cx + 17 * S, cy - 44 * S], fill=(244, 208, 104, 255))
        d.pieslice([cx - 11 * S, cy - 58 * S, cx + 11 * S, cy - 42 * S], start=180, end=360, fill=INK)
        d.pieslice([cx - 9 * S, cy - 56 * S, cx + 9 * S, cy - 44 * S], start=180, end=360, fill=(244, 208, 104, 255))
        d.line([(cx - 9 * S, cy - 46 * S), (cx + 9 * S, cy - 46 * S)], fill=(52, 56, 64, 255), width=3 * S)


def draw_villager_cell(d: ImageDraw.ImageDraw, ox: int, oy: int, S: int, shirt_col, holding_letter: bool) -> None:
    cx, cy = ox + 32 * S, oy + 58 * S
    d.line([(cx - 5 * S, cy - 14 * S), (cx - 7 * S, cy)], fill=INK, width=3 * S)
    d.line([(cx + 5 * S, cy - 14 * S), (cx + 7 * S, cy)], fill=INK, width=3 * S)
    d.rounded_rectangle([cx - 10 * S, cy - 32 * S, cx + 10 * S, cy - 12 * S], radius=5 * S, fill=INK)
    d.rounded_rectangle([cx - 8 * S, cy - 30 * S, cx + 8 * S, cy - 14 * S], radius=4 * S, fill=shirt_col)
    d.line([(cx - 9 * S, cy - 24 * S), (cx - 17 * S, cy - 18 * S)], fill=INK, width=3 * S)
    d.line([(cx + 9 * S, cy - 24 * S), (cx + 18 * S, cy - 32 * S)], fill=INK, width=3 * S)
    if holding_letter:
        wobbly_poly(d, [(cx + 13 * S, cy - 42 * S), (cx + 26 * S, cy - 42 * S),
                        (cx + 26 * S, cy - 32 * S), (cx + 13 * S, cy - 32 * S)],
                    fill=(255, 250, 236, 255), outline=INK, width=2 * S, seed=ox)
        d.line([(cx + 13 * S, cy - 42 * S), (cx + 19 * S, cy - 36 * S), (cx + 26 * S, cy - 42 * S)],
               fill=(228, 92, 82, 255), width=2 * S)
    d.ellipse([cx - 12 * S, cy - 50 * S, cx + 12 * S, cy - 26 * S], fill=INK)
    d.ellipse([cx - 10 * S, cy - 48 * S, cx + 10 * S, cy - 28 * S], fill=(252, 246, 236, 255))
    d.ellipse([cx - 5 * S, cy - 40 * S, cx - 2 * S, cy - 37 * S], fill=INK)
    d.ellipse([cx + 2 * S, cy - 40 * S, cx + 5 * S, cy - 37 * S], fill=INK)
    d.arc([cx - 4 * S, cy - 36 * S, cx + 4 * S, cy - 31 * S], start=10, end=170, fill=INK, width=2 * S)
    d.ellipse([cx - 19 * S, cy - 50 * S, cx + 19 * S, cy - 42 * S], fill=INK)
    d.ellipse([cx - 17 * S, cy - 48 * S, cx + 17 * S, cy - 44 * S], fill=(244, 208, 104, 255))
    d.pieslice([cx - 11 * S, cy - 58 * S, cx + 11 * S, cy - 42 * S], start=180, end=360, fill=INK)
    d.pieslice([cx - 9 * S, cy - 56 * S, cx + 9 * S, cy - 44 * S], start=180, end=360, fill=(244, 208, 104, 255))
    d.line([(cx - 9 * S, cy - 46 * S), (cx + 9 * S, cy - 46 * S)], fill=(228, 86, 78, 255), width=3 * S)


def build_sprites() -> None:
    S = 2
    cell = 64 * S
    sheet = Image.new("RGBA", (cell * 4, cell * 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(sheet)

    def cell_xy(idx: int) -> tuple[int, int]:
        return (idx % 4) * cell, (idx // 4) * cell

    # 0: spr_pirate_scout (Black tricorn hat, red stripes, cutlass sword)
    draw_pirate_cell(d, *cell_xy(0), S, (52, 56, 66, 255), (228, 88, 78, 255), prop="sword", decoy=False)
    # 1: spr_pirate_jumper (Purple hat, orange stripes, bomb prop!)
    draw_pirate_cell(d, *cell_xy(1), S, (148, 82, 182, 255), (242, 168, 64, 255), prop="bomb", decoy=False)
    # 2: spr_pirate_disguised (Straw hat + eye-patch + gold coin)
    draw_pirate_cell(d, *cell_xy(2), S, (244, 208, 104, 255), (118, 178, 224, 255), prop="coin", decoy=True)
    # 3: spr_villager_postman (Straw hat villager with red shirt & letter)
    draw_villager_cell(d, *cell_xy(3), S, (228, 92, 82, 255), holding_letter=True)
    # 4: spr_villager_blue (Straw hat villager with sky-blue shirt)
    draw_villager_cell(d, *cell_xy(4), S, (118, 178, 224, 255), holding_letter=False)

    # 5: spr_cottage_blue
    ox, oy = cell_xy(5)
    wobbly_poly(d, [(ox + 38 * S, oy + 6 * S), (ox + 48 * S, oy + 6 * S),
                    (ox + 48 * S, oy + 26 * S), (ox + 38 * S, oy + 26 * S)],
                fill=(238, 232, 218, 255), outline=INK, width=3 * S, seed=51)
    wobbly_poly(d, [(ox + 10 * S, oy + 24 * S), (ox + 54 * S, oy + 24 * S),
                    (ox + 54 * S, oy + 60 * S), (ox + 10 * S, oy + 60 * S)],
                fill=(250, 246, 236, 255), outline=INK, width=3 * S, seed=52)
    wobbly_poly(d, [(ox + 32 * S, oy + 4 * S), (ox + 60 * S, oy + 26 * S), (ox + 4 * S, oy + 26 * S)],
                fill=(152, 188, 224, 255), outline=INK, width=3 * S, seed=53)
    wobbly_poly(d, [(ox + 16 * S, oy + 32 * S), (ox + 32 * S, oy + 32 * S),
                    (ox + 32 * S, oy + 48 * S), (ox + 16 * S, oy + 48 * S)],
                fill=(250, 214, 96, 255), outline=INK, width=2 * S, seed=54)
    d.line([(ox + 24 * S, oy + 32 * S), (ox + 24 * S, oy + 48 * S)], fill=INK, width=2 * S)
    d.line([(ox + 16 * S, oy + 40 * S), (ox + 32 * S, oy + 40 * S)], fill=INK, width=2 * S)
    wobbly_poly(d, [(ox + 36 * S, oy + 36 * S), (ox + 48 * S, oy + 36 * S),
                    (ox + 48 * S, oy + 60 * S), (ox + 36 * S, oy + 60 * S)],
                fill=(212, 166, 126, 255), outline=INK, width=2 * S, seed=55)

    # 6: spr_cottage_coral
    ox, oy = cell_xy(6)
    wobbly_poly(d, [(ox + 12 * S, oy + 26 * S), (ox + 52 * S, oy + 26 * S),
                    (ox + 52 * S, oy + 60 * S), (ox + 12 * S, oy + 60 * S)],
                fill=(250, 246, 236, 255), outline=INK, width=3 * S, seed=61)
    wobbly_poly(d, [(ox + 32 * S, oy + 6 * S), (ox + 58 * S, oy + 28 * S), (ox + 6 * S, oy + 28 * S)],
                fill=(232, 116, 102, 255), outline=INK, width=3 * S, seed=62)
    wobbly_poly(d, [(ox + 24 * S, oy + 34 * S), (ox + 40 * S, oy + 34 * S),
                    (ox + 40 * S, oy + 50 * S), (ox + 24 * S, oy + 50 * S)],
                fill=(250, 214, 96, 255), outline=INK, width=2 * S, seed=63)
    d.line([(ox + 32 * S, oy + 34 * S), (ox + 32 * S, oy + 50 * S)], fill=INK, width=2 * S)
    d.line([(ox + 24 * S, oy + 42 * S), (ox + 40 * S, oy + 42 * S)], fill=INK, width=2 * S)

    # 7: spr_tree_diamond
    ox, oy = cell_xy(7)
    tx, ty = ox + 32 * S, oy + 60 * S
    wobbly_poly(d, [(tx - 5 * S, ty - 44 * S), (tx + 5 * S, ty - 44 * S),
                    (tx + 8 * S, ty), (tx - 8 * S, ty)],
                fill=(166, 208, 144, 255), outline=INK, width=3 * S, seed=71)
    for dx, dy, ang in ((tx - 16 * S, ty - 36 * S, -26), (tx + 16 * S, ty - 34 * S, 26),
                        (tx - 14 * S, ty - 20 * S, -20), (tx + 15 * S, ty - 18 * S, 20),
                        (tx, ty - 48 * S, 0)):
        rad = math.radians(ang)
        cs, sn = math.cos(rad), math.sin(rad)
        lpts = [(dx + (lx * cs - ly * sn) * S, dy + (lx * sn + ly * cs) * S)
                for lx, ly in ((0, -12), (7, 0), (0, 12), (-7, 0))]
        wobbly_poly(d, lpts, fill=(134, 204, 120, 255), outline=INK, width=2 * S, seed=int(dx + dy))
        d.line([lpts[0], lpts[2]], fill=INK, width=2 * S)

    # 8: spr_lighthouse_top
    ox, oy = cell_xy(8)
    lx = ox + 32 * S
    wobbly_poly(d, [(lx - 18 * S, oy + 38 * S), (lx + 18 * S, oy + 38 * S),
                    (lx + 20 * S, oy + 62 * S), (lx - 20 * S, oy + 62 * S)],
                fill=(228, 108, 92, 255), outline=INK, width=3 * S, seed=81)
    wobbly_poly(d, [(lx - 22 * S, oy + 32 * S), (lx + 22 * S, oy + 32 * S),
                    (lx + 18 * S, oy + 38 * S), (lx - 18 * S, oy + 38 * S)],
                fill=(218, 182, 146, 255), outline=INK, width=3 * S, seed=82)
    wobbly_poly(d, [(lx - 14 * S, oy + 16 * S), (lx + 14 * S, oy + 16 * S),
                    (lx + 14 * S, oy + 32 * S), (lx - 14 * S, oy + 32 * S)],
                fill=(252, 236, 128, 255), outline=INK, width=3 * S, seed=83)
    wobbly_poly(d, [(lx, oy + 2 * S), (lx + 20 * S, oy + 16 * S), (lx - 20 * S, oy + 16 * S)],
                fill=(228, 108, 92, 255), outline=INK, width=3 * S, seed=84)

    # 9: spr_lighthouse_base
    ox, oy = cell_xy(9)
    lx = ox + 32 * S
    for idx_s, (y0, y1, col) in enumerate(((2, 22, (250, 246, 236, 255)),
                                           (22, 42, (228, 108, 92, 255)),
                                           (42, 62, (250, 246, 236, 255)))):
        w0 = (20 + idx_s) * S
        w1 = (21 + idx_s) * S
        wobbly_poly(d, [(lx - w0, oy + y0 * S), (lx + w0, oy + y0 * S),
                        (lx + w1, oy + y1 * S), (lx - w1, oy + y1 * S)],
                    fill=col, outline=INK, width=3 * S, seed=90 + idx_s)
    wobbly_poly(d, [(lx - 4 * S, oy + 7 * S), (lx + 4 * S, oy + 7 * S),
                    (lx + 4 * S, oy + 17 * S), (lx - 4 * S, oy + 17 * S)],
                fill=(248, 214, 102, 255), outline=INK, width=2 * S, seed=95)

    # 10: spr_gate_barricade
    ox, oy = cell_xy(10)
    wobbly_poly(d, [(ox + 10 * S, oy + 4 * S), (ox + 54 * S, oy + 4 * S),
                    (ox + 54 * S, oy + 60 * S), (ox + 10 * S, oy + 60 * S)],
                fill=(238, 128, 108, 255), outline=INK, width=3 * S, seed=101)
    wobbly_line(d, (ox + 15 * S, oy + 10 * S), (ox + 49 * S, oy + 54 * S), width=4 * S, seed=102)
    wobbly_line(d, (ox + 49 * S, oy + 10 * S), (ox + 15 * S, oy + 54 * S), width=4 * S, seed=103)
    d.rounded_rectangle([ox + 16 * S, oy + 26 * S, ox + 48 * S, oy + 38 * S], radius=4 * S, fill=(250, 244, 228, 255), outline=INK, width=2 * S)

    # 11: spr_sun
    ox, oy = cell_xy(11)
    sx, sy = ox + 32 * S, oy + 32 * S
    for i in range(12):
        ang = i * math.pi / 6.0
        wobbly_line(d, (sx + math.cos(ang) * 20 * S, sy + math.sin(ang) * 20 * S),
                    (sx + math.cos(ang) * 28 * S, sy + math.sin(ang) * 28 * S),
                    fill=(242, 182, 58, 255), width=3 * S, seed=110 + i)
    d.ellipse([sx - 17 * S, sy - 17 * S, sx + 17 * S, sy + 17 * S], fill=INK)
    d.ellipse([sx - 15 * S, sy - 15 * S, sx + 15 * S, sy + 15 * S], fill=(252, 214, 78, 255))

    # 12: spr_cloud
    ox, oy = cell_xy(12)
    cx, cy = ox + 32 * S, oy + 34 * S
    for dx, dy, rad in ((-14, 3, 11), (-4, -3, 14), (10, 0, 12), (18, 4, 9), (2, 5, 12)):
        d.ellipse([cx + (dx - rad - 2) * S, cy + (dy - rad - 2) * S,
                   cx + (dx + rad + 2) * S, cy + (dy + rad + 2) * S], fill=INK)
    for dx, dy, rad in ((-14, 3, 11), (-4, -3, 14), (10, 0, 12), (18, 4, 9), (2, 5, 12)):
        d.ellipse([cx + (dx - rad) * S, cy + (dy - rad) * S,
                   cx + (dx + rad) * S, cy + (dy + rad) * S], fill=(252, 253, 255, 255))

    # 13: spr_treasure
    ox, oy = cell_xy(13)
    wobbly_poly(d, [(ox + 10 * S, oy + 22 * S), (ox + 54 * S, oy + 22 * S),
                    (ox + 54 * S, oy + 56 * S), (ox + 10 * S, oy + 56 * S)],
                fill=(244, 196, 78, 255), outline=INK, width=3 * S, seed=131)
    wobbly_poly(d, [(ox + 8 * S, oy + 14 * S), (ox + 56 * S, oy + 14 * S),
                    (ox + 54 * S, oy + 26 * S), (ox + 10 * S, oy + 26 * S)],
                fill=(228, 148, 64, 255), outline=INK, width=3 * S, seed=132)
    wobbly_poly(d, [(ox + 28 * S, oy + 22 * S), (ox + 36 * S, oy + 22 * S),
                    (ox + 36 * S, oy + 34 * S), (ox + 28 * S, oy + 34 * S)],
                fill=(252, 244, 214, 255), outline=INK, width=2 * S, seed=133)

    # 14: spr_bush_hideout
    ox, oy = cell_xy(14)
    cx, cy = ox + 32 * S, oy + 42 * S
    for dx, dy, rad in ((-15, 4, 13), (-5, -6, 16), (11, -3, 14), (17, 5, 12), (2, 6, 14)):
        d.ellipse([cx + (dx - rad - 2) * S, cy + (dy - rad - 2) * S,
                   cx + (dx + rad + 2) * S, cy + (dy + rad + 2) * S], fill=INK)
    for dx, dy, rad in ((-15, 4, 13), (-5, -6, 16), (11, -3, 14), (17, 5, 12), (2, 6, 14)):
        d.ellipse([cx + (dx - rad) * S, cy + (dy - rad) * S,
                   cx + (dx + rad) * S, cy + (dy + rad) * S], fill=(142, 208, 126, 255))
    for bx, by in ((-10, -2), (8, -5), (14, 6)):
        d.ellipse([cx + (bx - 3) * S, cy + (by - 3) * S, cx + (bx + 3) * S, cy + (by + 3) * S], fill=(244, 152, 72, 255), outline=INK, width=1 * S)

    # 15: spr_barrel_obstacle
    ox, oy = cell_xy(15)
    cx, cy = ox + 32 * S, oy + 34 * S
    wobbly_poly(d, [(cx - 16 * S, cy - 18 * S), (cx + 16 * S, cy - 18 * S),
                    (cx + 18 * S, cy + 18 * S), (cx - 18 * S, cy + 18 * S)],
                fill=(204, 148, 98, 255), outline=INK, width=3 * S, seed=151)
    d.line([(cx - 17 * S, cy - 8 * S), (cx + 17 * S, cy - 8 * S)], fill=INK, width=3 * S)
    d.line([(cx - 17 * S, cy + 8 * S), (cx + 17 * S, cy + 8 * S)], fill=INK, width=3 * S)

    final_sheet = sheet.resize((256, 256), RESAMPLING.LANCZOS)
    px = final_sheet.load()
    for y in range(256):
        for x in range(256):
            r, g, b, a = px[x, y]
            px[x, y] = (0, 0, 0, 0) if a < 110 else (r, g, b, 255)
    save_atomic(final_sheet, ROOT / "story_sprites_source.png")

    names = [
        "spr_pirate_scout", "spr_pirate_jumper", "spr_pirate_disguised", "spr_villager_postman",
        "spr_villager_blue", "spr_cottage_blue", "spr_cottage_coral", "spr_tree_diamond",
        "spr_lighthouse_top", "spr_lighthouse_base", "spr_gate_barricade", "spr_sun",
        "spr_cloud", "spr_treasure", "spr_bush_hideout", "spr_barrel_obstacle",
    ]
    cfg = {
        "image": "story_sprites_source.png",
        "columns": 4,
        "rows": 4,
        "output_cell": 64,
        "output_columns": 4,
        "alpha_mode": "binary",
        "padding": 0,
        "frames": [{"name": name, "source_index": i} for i, name in enumerate(names)],
    }
    (ROOT / "story_sprites.json").write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")


def build_chinese_font() -> None:
    text_chars = (
        " ?0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
        ":+-/!%x().,"
        "海盗保卫战灯塔小岛手绘日常"
        "第一二三幕关波次得分生命剩余时间步数连击轮挑战最高纪录"
        "划动木闸堵截揪出伪装种树建房围困"
        "上下滑动红木闸把海盗弹进水里放行草帽村民"
        "点击窗户树冠草丛灯塔找出名潜伏"
        "点击圆石建造小屋或大树别让逃到岸边"
        "落水抓获误伤村民溜走成功逃脱"
        "点击屏幕开始巡逻下一关重新挑战"
        "完美守卫大获全胜失守通关记录"
        "海风阵风顺风加速浓雾迷雾跳跃精锐双线突袭晴朗微风涨潮变数"
        "侦察线索目标持弯刀紫色头巾黑帽偷戴草帽抱炸药桶拿金币"
        "海岛工坊选择项科技升级下一轮"
        "加宽木闸撞击范围更易弹飞海盗"
        "鹰眼探照灯自动揭露一名真凶并延时"
        "预筑路障开局额外多放两座小屋"
        "加固宝库立刻恢复一点生命并加分"
        "宝藏失守巡逻结束最终守卫轮数"
        "村民通过成功围困跳越换道哎呀本幕"
    )
    all_codes = sorted(set(ord(ch) for ch in text_chars))
    cols = 16
    rows_cnt = (len(all_codes) + cols - 1) // cols
    cell = 20
    atlas_w, atlas_h = cols * cell, rows_cnt * cell
    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))

    font_path = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc")
    font = ImageFont.truetype(str(font_path), 16, index=2) if font_path.is_file() else ImageFont.load_default()

    entries = []
    for idx, cp in enumerate(all_codes):
        col, row = idx % cols, idx // cols
        cx, cy = col * cell, row * cell
        ch = chr(cp)
        if ch == " ":
            entries.append((cp, cx + 1, cy + 1, 1, 1, 0, 2, 6))
            continue
        mask = Image.new("L", (cell, cell), 0)
        md = ImageDraw.Draw(mask)
        md.text((2, 1), ch, font=font, fill=255)
        bbox = mask.getbbox()
        if not bbox:
            entries.append((cp, cx + 1, cy + 1, 1, 1, 0, 2, 8))
            continue
        mpx = mask.load()
        apx = atlas.load()
        for y in range(cell):
            for x in range(cell):
                if mpx[x, y] >= 115:
                    apx[cx + x, cy + y] = (255, 255, 255, 255)
        gw = max(1, min(cell - bbox[0], bbox[2] - bbox[0]))
        gh = max(1, min(cell - bbox[1], bbox[3] - bbox[1]))
        adv = gw + 1 if cp < 128 else 16
        entries.append((cp, cx + bbox[0], cy + bbox[1], gw, gh, 0, bbox[1], adv))

    save_atomic(atlas, ROOT / "pirate_zh_source.png")
    cfg = {
        "image": "pirate_zh_source.png",
        "columns": cols,
        "rows": rows_cnt,
        "output_cell": cell,
        "output_columns": cols,
        "alpha_mode": "binary",
        "padding": 0,
        "frames": [{"name": f"pzh_{i}", "source_index": i} for i in range(cols * rows_cnt)],
    }
    (ROOT / "pirate_zh.json").write_text(json.dumps(cfg, indent=2) + "\n", encoding="utf-8")

    lines = [
        'info face="StorybookZh" size=16',
        f"common lineHeight=18 base=14 scaleW={atlas_w} scaleH={atlas_h} pages=1",
        'page id=0 file="pirate_zh.png"',
        f"chars count={len(entries)}",
    ]
    for cp, gx, gy, gw, gh, ox, oy, adv in entries:
        lines.append(
            f"char id={cp} x={gx} y={gy} width={gw} height={gh} "
            f"xoffset={ox} yoffset={oy} xadvance={adv} page=0"
        )
    (ROOT / "pirate_zh.fnt").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    build_backdrop()
    build_sprites()
    build_chinese_font()
