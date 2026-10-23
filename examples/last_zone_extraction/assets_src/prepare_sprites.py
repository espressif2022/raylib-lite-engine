#!/usr/bin/env python3
"""Deterministically build the Direction A Cel-Shaded Tactical assets for Last Zone."""
from pathlib import Path
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

root = Path(__file__).resolve().parent


def save_atomic(image: Image.Image, destination: Path) -> None:
    temporary = destination.with_name(f"{destination.name}.tmp.{os.getpid()}.png")
    image.save(temporary, format="PNG")
    os.replace(temporary, destination)


enemy_cell = 96
raw_enemy = Image.new("RGBA", (enemy_cell * 7, enemy_cell))
weapon_cell = 176
raw_weapon = Image.new("RGBA", (weapon_cell * 2, weapon_cell))
resampling = getattr(Image, "Resampling", Image)
run_strip = Image.open(root / "enemy_run_strip_source.png").convert("RGBA")
cell_width = run_strip.width // 3
sprites = [run_strip.crop((i * cell_width, 0,
                          (i + 1) * cell_width if i < 2 else run_strip.width,
                          run_strip.height)) for i in range(3)]
processed = []
for index, sprite in enumerate(sprites):
    alpha_bounds = sprite.getchannel("A").getbbox()
    if not alpha_bounds:
        raise RuntimeError(f"generated sprite cell {index} has no alpha content")
    sprite = sprite.crop(alpha_bounds)
    sprite.thumbnail((enemy_cell - 8, enemy_cell - 8), resampling.LANCZOS)
    processed.append(sprite)
    raw_enemy.alpha_composite(sprite,
        (index * enemy_cell + (enemy_cell - sprite.width) // 2,
         enemy_cell - 4 - sprite.height))


def paste_variant(index: int, source: Image.Image, tint: tuple[float, float, float]) -> None:
    variant = source.copy()
    pixels = variant.load()
    for y in range(variant.height):
        for x in range(variant.width):
            r, g, b, a = pixels[x, y]
            if a < 16:
                continue
            pixels[x, y] = (min(255, int(r * tint[0])), min(255, int(g * tint[1])),
                            min(255, int(b * tint[2])), a)
    raw_enemy.alpha_composite(variant,
        (index * enemy_cell + (enemy_cell - variant.width) // 2,
         enemy_cell - 4 - variant.height))


paste_variant(3, processed[1], (1.15, 0.95, 0.72))
paste_variant(4, processed[0], (1.45, 0.55, 0.45))
paste_variant(5, processed[2], (1.20, 1.05, 0.65))
down = processed[1].rotate(90, expand=True, resample=resampling.BICUBIC)
down.thumbnail((enemy_cell - 8, enemy_cell // 2), resampling.LANCZOS)
paste_variant(6, down, (0.88, 0.78, 0.70))

# Cel-shade the 7 enemy frames: 2px dark comic ink outline + high-contrast
# crimson/vermilion tactical vest + glowing yellow ballistic visor.
enemy_sheet = Image.new("RGBA", raw_enemy.size, (0, 0, 0, 0))
opx = raw_enemy.load()
npx = enemy_sheet.load()
for f in range(7):
    x_off = f * enemy_cell
    mask = [[opx[x_off + x, y][3] >= 96 for y in range(enemy_cell)] for x in range(enemy_cell)]
    ys = [y for x in range(enemy_cell) for y in range(enemy_cell) if mask[x][y]]
    min_y, max_y = (min(ys), max(ys)) if ys else (0, enemy_cell - 1)
    span_y = max(1, max_y - min_y)
    for x in range(enemy_cell):
        for y in range(enemy_cell):
            if not mask[x][y]:
                near = False
                for dx in (-2, -1, 0, 1, 2):
                    for dy in (-2, -1, 0, 1, 2):
                        if dx * dx + dy * dy <= 4:
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < enemy_cell and 0 <= ny < enemy_cell and mask[nx][ny]:
                                near = True
                                break
                    if near:
                        break
                if near:
                    npx[x_off + x, y] = (14, 18, 24, 255)
                continue

            r, g, b, _ = opx[x_off + x, y]
            lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
            rel_y = (y - min_y) / span_y
            if f == 4:
                if lum > 0.52:
                    npx[x_off + x, y] = (255, 232, 110, 255)
                elif lum > 0.28:
                    npx[x_off + x, y] = (255, 86, 64, 255)
                else:
                    npx[x_off + x, y] = (168, 28, 36, 255)
                continue
            if lum < 0.15:
                npx[x_off + x, y] = (20, 24, 32, 255)
                continue
            if f < 6:
                if rel_y < 0.22:
                    row_xs = [xx for xx in range(enemy_cell) if mask[xx][y]]
                    hx0, hx1 = min(row_xs), max(row_xs)
                    h_frac = (x - hx0) / max(1, hx1 - hx0)
                    if 0.09 <= rel_y <= 0.18 and 0.22 <= h_frac <= 0.80:
                        npx[x_off + x, y] = (255, 224, 68, 255) if lum > 0.26 else (232, 142, 36, 255)
                    else:
                        npx[x_off + x, y] = (86, 102, 122, 255) if lum > 0.44 else (48, 58, 72, 255)
                elif rel_y < 0.56:
                    row_xs = [xx for xx in range(enemy_cell) if mask[xx][y]]
                    tx0, tx1 = min(row_xs), max(row_xs)
                    t_frac = (x - tx0) / max(1, tx1 - tx0)
                    is_gun = (r < 92 and g < 92 and abs(r - g) < 12 and rel_y > 0.30)
                    if is_gun:
                        npx[x_off + x, y] = (124, 138, 156, 255) if lum > 0.40 else (42, 50, 62, 255)
                    elif 0.20 <= t_frac <= 0.80:
                        if lum > 0.50:
                            npx[x_off + x, y] = (248, 112, 68, 255)
                        elif lum > 0.28:
                            npx[x_off + x, y] = (212, 58, 48, 255)
                        else:
                            npx[x_off + x, y] = (138, 32, 34, 255)
                    else:
                        npx[x_off + x, y] = (78, 92, 108, 255) if lum > 0.40 else (46, 54, 66, 255)
                else:
                    if lum > 0.46:
                        npx[x_off + x, y] = (96, 108, 126, 255)
                    elif lum > 0.26:
                        npx[x_off + x, y] = (58, 68, 82, 255)
                    else:
                        npx[x_off + x, y] = (34, 40, 50, 255)
            else:
                if lum > 0.46:
                    npx[x_off + x, y] = (212, 78, 62, 255)
                elif lum > 0.26:
                    npx[x_off + x, y] = (72, 82, 96, 255)
                else:
                    npx[x_off + x, y] = (38, 44, 56, 255)

save_atomic(enemy_sheet, root / "enemy_run.png")

rifle = Image.open(root / "tactical_bolt_rifle_source.png").convert("RGBA")
alpha_bounds = rifle.getchannel("A").getbbox()
if not alpha_bounds:
    raise RuntimeError("98K sprite has no alpha content")
rifle = rifle.crop(alpha_bounds)
rifle.thumbnail((weapon_cell - 10, weapon_cell - 10), resampling.LANCZOS)
raw_weapon.alpha_composite(rifle, (weapon_cell - rifle.width - 6,
                                   weapon_cell - rifle.height - 4))
bolted = rifle.rotate(-14, expand=True, resample=resampling.BICUBIC)
bolted.thumbnail((weapon_cell - 6, weapon_cell - 6), resampling.LANCZOS)
raw_weapon.alpha_composite(bolted, (weapon_cell + 16,
                                    weapon_cell - bolted.height + 10))

# Cel-shade the first-person sniper rifle: crisp dark ink outline, 3-tone
# gunmetal + titanium edge highlights, and a glowing cyan optic scope lens.
weapon_sheet = Image.new("RGBA", raw_weapon.size, (0, 0, 0, 0))
wopx = raw_weapon.load()
wnpx = weapon_sheet.load()
for f in range(2):
    x_off = f * weapon_cell
    mask = [[wopx[x_off + x, y][3] >= 96 for y in range(weapon_cell)] for x in range(weapon_cell)]
    for x in range(weapon_cell):
        for y in range(weapon_cell):
            if not mask[x][y]:
                near = False
                for dx in (-2, -1, 0, 1, 2):
                    for dy in (-2, -1, 0, 1, 2):
                        if dx * dx + dy * dy <= 4:
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < weapon_cell and 0 <= ny < weapon_cell and mask[nx][ny]:
                                near = True
                                break
                    if near:
                        break
                if near:
                    wnpx[x_off + x, y] = (12, 16, 22, 255)
                continue

            r, g, b, _ = wopx[x_off + x, y]
            lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
            is_sleeve = (g > r + 6 and g > b + 8 and y > weapon_cell * 0.48)
            is_skin = (r > g + 14 and g > b + 8 and lum > 0.30)
            if is_skin:
                wnpx[x_off + x, y] = (224, 172, 136, 255) if lum > 0.50 else (178, 126, 94, 255)
            elif is_sleeve:
                if lum > 0.40:
                    wnpx[x_off + x, y] = (72, 118, 112, 255)
                elif lum > 0.22:
                    wnpx[x_off + x, y] = (48, 86, 82, 255)
                else:
                    wnpx[x_off + x, y] = (28, 52, 52, 255)
            else:
                if lum > 0.48:
                    wnpx[x_off + x, y] = (182, 198, 216, 255)
                elif lum > 0.26:
                    wnpx[x_off + x, y] = (96, 112, 132, 255)
                elif lum > 0.13:
                    wnpx[x_off + x, y] = (50, 58, 72, 255)
                else:
                    wnpx[x_off + x, y] = (22, 26, 34, 255)

    wd = ImageDraw.Draw(weapon_sheet)
    sx, sy = x_off + (80 if f == 0 else 82), (76 if f == 0 else 78)
    wd.ellipse([sx - 15, sy - 15, sx + 15, sy + 15], fill=(16, 20, 28, 255), outline=(148, 168, 190, 255), width=2)
    wd.ellipse([sx - 11, sy - 11, sx + 11, sy + 11], fill=(28, 192, 218, 255))
    wd.ellipse([sx - 7, sy - 7, sx - 1, sy - 1], fill=(215, 252, 255, 255))
    wd.line([(sx - 9, sy), (sx + 9, sy)], fill=(14, 72, 88, 255), width=1)
    wd.line([(sx, sy - 9), (sx, sy + 9)], fill=(14, 72, 88, 255), width=1)
    wd.rectangle([sx - 1, sy - 1, sx + 1, sy + 1], fill=(255, 92, 82, 255))
    if f == 1:
        wd.rounded_rectangle([sx + 14, sy + 10, sx + 26, sy + 18], radius=2, fill=(255, 206, 72, 255), outline=(20, 24, 32, 255))

save_atomic(weapon_sheet, root / "weapon.png")

# Binary-alpha HUD. Corners stay A=0 so the world shows through the rings.
controls = Image.new("RGBA", (192, 96), (0, 0, 0, 0))
draw = ImageDraw.Draw(controls)
draw.ellipse((4, 4, 92, 92), outline=(120, 168, 158, 255), width=4)
draw.ellipse((14, 14, 82, 82), outline=(48, 72, 68, 255), width=2)
draw.ellipse((100, 4, 188, 92), outline=(196, 118, 78, 255), width=4)
draw.ellipse((132, 36, 156, 60), fill=(246, 224, 201, 255))
draw.line((144, 18, 144, 32), fill=(255, 239, 214, 255), width=3)
draw.line((144, 64, 144, 78), fill=(255, 239, 214, 255), width=3)
draw.line((116, 48, 130, 48), fill=(255, 239, 214, 255), width=3)
draw.line((158, 48, 172, 48), fill=(255, 239, 214, 255), width=3)
save_atomic(controls, root / "controls.png")

# Five compact 360-degree horizon bands.
panorama_names = ("dock", "depot", "command", "ghost", "run")
panorama_cell = 128
panorama_inner = panorama_cell - 4
environment = Image.new("RGB", (panorama_cell * 2,
                                panorama_cell * len(panorama_names)), (18, 24, 28))
for row, name in enumerate(panorama_names):
    source = Image.open(root / f"panorama_{name}_source.png").convert("RGB")
    source = ImageOps.fit(source, (1536, 512), method=resampling.LANCZOS,
                          centering=(0.5, 0.5))
    rolled = ImageChops.offset(source, source.width // 2, 0)
    seam_width = 160
    seam_left = source.width // 2 - seam_width // 2
    seam = rolled.crop((seam_left, 0, seam_left + seam_width, source.height))
    seam = seam.filter(ImageFilter.GaussianBlur(radius=18))
    mask = Image.new("L", (seam_width, source.height), 0)
    mask_px = mask.load()
    for x in range(seam_width):
        edge = min(x, seam_width - 1 - x)
        alpha = min(255, edge * 255 // (seam_width // 3))
        for y in range(source.height):
            mask_px[x, y] = alpha
    rolled.paste(seam, (seam_left, 0), mask)
    band = ImageOps.fit(rolled, (panorama_inner * 2, panorama_cell - 4),
                        method=resampling.LANCZOS, centering=(0.5, 0.43))
    top = row * panorama_cell + 2
    environment.paste(band.crop((0, 0, panorama_inner, band.height)), (2, top))
    environment.paste(band.crop((panorama_inner, 0, panorama_inner * 2,
                                 band.height)), (panorama_cell + 2, top))
save_atomic(environment, root / "tactical_panoramas.png")

# Direction A Cel-Shaded Low-Frequency Materials (512x128 RGB)
materials = Image.new("RGB", (512, 128), (40, 44, 52))
md = ImageDraw.Draw(materials)

# Cell 0 (0..127): wall_concrete (Precast bunker panels + bevels + rivets + dark footing)
for y in range(128):
    v = int(136 - (y / 127.0) * 22)
    md.line([(0, y), (127, y)], fill=(v, v + 4, v + 10))
md.rectangle([0, 0, 127, 10], fill=(72, 78, 88))
md.line([(0, 11), (127, 11)], fill=(164, 172, 184), width=2)
md.rectangle([0, 112, 127, 127], fill=(42, 46, 54))
md.line([(0, 111), (127, 111)], fill=(28, 32, 38), width=2)
for gx in (0, 64):
    md.line([(gx, 12), (gx, 110)], fill=(68, 74, 84), width=2)
    md.line([(gx + 2, 12), (gx + 2, 110)], fill=(158, 166, 178), width=2)
md.line([(0, 62), (127, 62)], fill=(72, 78, 88), width=2)
md.line([(0, 64), (127, 64)], fill=(156, 164, 176), width=2)
for rx in (10, 54, 74, 118):
    for ry in (20, 52, 74, 102):
        md.rectangle([rx - 2, ry - 2, rx + 2, ry + 2], fill=(58, 64, 74))
        md.point((rx - 1, ry - 1), fill=(176, 184, 196))

# Cell 1 (128..255): wall_brick (Industrial masonry + yellow/black hazard waist band)
x0 = 128
for y in range(128):
    if y < 58:
        md.line([(x0, y), (x0 + 127, y)], fill=(142, 128, 118))
    elif y < 74:
        md.line([(x0, y), (x0 + 127, y)], fill=(36, 38, 44))
    else:
        md.line([(x0, y), (x0 + 127, y)], fill=(96, 86, 82))
for row_idx, ry in enumerate((0, 18, 36, 76, 94, 112)):
    md.line([(x0, ry), (x0 + 127, ry)], fill=(64, 58, 56), width=2)
    md.line([(x0, ry + 2), (x0 + 127, ry + 2)], fill=(166, 152, 142) if ry < 58 else (118, 106, 100), width=1)
    offset = 16 if (row_idx & 1) else 0
    for bx in range(offset, 128, 32):
        md.line([(x0 + bx, ry), (x0 + bx, min(127, ry + 18))], fill=(68, 62, 60), width=2)
for hx in range(-16, 144, 16):
    pts = [(x0 + max(0, min(127, hx)), 72),
           (x0 + max(0, min(127, hx + 8)), 72),
           (x0 + max(0, min(127, hx + 16)), 59),
           (x0 + max(0, min(127, hx + 8)), 59)]
    md.polygon(pts, fill=(238, 186, 52))
md.rectangle([x0, 56, x0 + 127, 58], fill=(28, 30, 36))
md.rectangle([x0, 73, x0 + 127, 75], fill=(28, 30, 36))
md.rectangle([x0, 114, x0 + 127, 127], fill=(42, 38, 38))

# Cell 2 (256..383): wall_container (Cel-shaded teal shipping container ribs + amber stripe)
x0 = 256
for rib in range(8):
    rx = x0 + rib * 16
    md.rectangle([rx, 0, rx + 9, 127], fill=(46, 118, 110))
    md.rectangle([rx + 10, 0, rx + 11, 127], fill=(82, 162, 152))
    md.rectangle([rx + 12, 0, rx + 15, 127], fill=(26, 74, 70))
md.rectangle([x0, 0, x0 + 127, 12], fill=(32, 86, 82))
md.line([(x0, 12), (x0 + 127, 12)], fill=(18, 48, 46), width=2)
md.rectangle([x0, 114, x0 + 127, 127], fill=(24, 54, 52))
md.rectangle([x0 + 12, 22, x0 + 115, 30], fill=(236, 188, 56))
md.rectangle([x0 + 12, 22, x0 + 115, 30], outline=(24, 58, 56), width=1)

# Cell 3 (384..511): floor_dirt (Zero-shimmer low-contrast tactical slate & deck slabs)
x0 = 384
md.rectangle([x0, 0, x0 + 127, 63], fill=(58, 64, 72))
for gx in (x0, x0 + 64):
    md.line([(gx, 0), (gx, 63)], fill=(50, 56, 64), width=2)
    md.line([(gx + 2, 0), (gx + 2, 63)], fill=(64, 70, 78), width=1)
for gy in (0, 32):
    md.line([(x0, gy), (x0 + 127, gy)], fill=(50, 56, 64), width=2)
    md.line([(x0, gy + 2), (x0 + 127, gy + 2)], fill=(64, 70, 78), width=1)

md.rectangle([x0, 64, x0 + 127, 127], fill=(76, 68, 60))
for gy in (64, 80, 96, 112):
    md.line([(x0, gy), (x0 + 127, gy)], fill=(66, 58, 50), width=2)
    md.line([(x0, gy + 2), (x0 + 127, gy + 2)], fill=(84, 76, 68), width=1)
for gx in (x0 + 32, x0 + 96):
    md.line([(gx, 64), (gx, 127)], fill=(66, 58, 50), width=2)

save_atomic(materials, root / "tactical_materials.png")

# Cel-Shaded Props (160x32 RGBA)
props = Image.new("RGBA", (160, 32), (0, 0, 0, 0))
pd = ImageDraw.Draw(props)
OUT = (16, 20, 28, 255)

# 0: pickup_ammo
pd.rounded_rectangle([3, 7, 28, 29], radius=4, fill=OUT)
pd.rounded_rectangle([5, 9, 26, 27], radius=3, fill=(68, 96, 58, 255))
pd.rectangle([5, 15, 26, 21], fill=(248, 198, 64, 255))
pd.rounded_rectangle([10, 4, 21, 9], radius=2, fill=OUT)
for bx in (9, 15, 21):
    pd.rectangle([bx, 12, bx + 2, 24], fill=(255, 224, 96, 255))

# 1: pickup_health
x0 = 32
pd.rounded_rectangle([x0 + 3, 6, x0 + 28, 29], radius=5, fill=OUT)
pd.rounded_rectangle([x0 + 5, 8, x0 + 26, 27], radius=4, fill=(238, 244, 252, 255))
pd.rectangle([x0 + 13, 11, x0 + 18, 24], fill=(232, 56, 68, 255))
pd.rectangle([x0 + 9, 15, x0 + 22, 20], fill=(232, 56, 68, 255))

# 2: prop_barrel
x0 = 64
pd.rounded_rectangle([x0 + 5, 2, x0 + 26, 30], radius=5, fill=OUT)
pd.rounded_rectangle([x0 + 7, 4, x0 + 24, 28], radius=3, fill=(218, 54, 48, 255))
pd.rectangle([x0 + 7, 12, x0 + 24, 19], fill=(252, 204, 64, 255))
pd.line([(x0 + 6, 9), (x0 + 25, 9)], fill=OUT, width=2)
pd.line([(x0 + 6, 22), (x0 + 25, 22)], fill=OUT, width=2)
pd.polygon([(x0 + 15, 13), (x0 + 19, 18), (x0 + 11, 18)], fill=OUT)

# 3: prop_bag
x0 = 96
pd.rounded_rectangle([x0 + 2, 12, x0 + 29, 29], radius=6, fill=OUT)
pd.rounded_rectangle([x0 + 4, 14, x0 + 27, 27], radius=4, fill=(176, 152, 112, 255))
pd.line([(x0 + 4, 20), (x0 + 27, 20)], fill=(118, 98, 68, 255), width=2)
pd.line([(x0 + 15, 14), (x0 + 15, 27)], fill=(118, 98, 68, 255), width=2)

# 4: pickup_armor
x0 = 128
pd.polygon([(x0 + 16, 3), (x0 + 28, 9), (x0 + 25, 24), (x0 + 16, 30), (x0 + 7, 24), (x0 + 4, 9)], fill=OUT)
pd.polygon([(x0 + 16, 5), (x0 + 26, 10), (x0 + 23, 23), (x0 + 16, 28), (x0 + 9, 23), (x0 + 6, 10)], fill=(48, 148, 232, 255))
pd.polygon([(x0 + 16, 8), (x0 + 22, 12), (x0 + 20, 21), (x0 + 16, 24), (x0 + 12, 21), (x0 + 10, 12)], fill=(118, 228, 255, 255))

save_atomic(props, root / "props.png")
