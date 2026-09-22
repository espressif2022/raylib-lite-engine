#!/usr/bin/env python3
"""Bake tomb-explorer INDEX8 tiles into an opaque 5x2 RGB PNG atlas."""
import math
import random
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent
SOURCE_SIZE = 128
SIZE = 64
COLUMNS = 5
ROWS = 2

RAMPS = ['gray', 'sand', 'ochre', 'brown', 'moss', 'teal', 'blue', 'gold', 'red',
         'skin', 'hair', 'cloth', 'leather', 'bone', 'water', 'white']
RAMP_BRIGHT = [
    (200, 200, 205), (225, 200, 150), (205, 150, 80), (140, 95, 60),
    (95, 140, 70), (60, 150, 140), (70, 95, 190), (240, 200, 70),
    (180, 55, 45), (230, 175, 135), (70, 45, 30), (50, 110, 150),
    (110, 70, 40), (235, 225, 200), (40, 90, 130), (255, 255, 255),
]


def index(ramp, step):
    step = max(0, min(15, int(round(step))))
    return RAMPS.index(ramp) * 16 + step


def ramp_rgb(palette_index, light=15):
    if palette_index == 0:
        return (0, 0, 0)
    ramp, step = divmod(palette_index, 16)
    light_scale = 24 + light * (256 - 24) // 15
    step_scale = 40 + step * (256 - 40) // 15
    scale = step_scale * light_scale // 256
    bright = RAMP_BRIGHT[ramp]
    r = bright[0] * scale // 256
    g = bright[1] * scale // 256
    b = bright[2] * scale // 256
    if light < 6:
        b = min(255, b + (6 - light) * 3)
    return (r, g, b)


def noise_grid(rng, cells):
    return [[rng.random() for _ in range(cells)] for _ in range(cells)]


def smooth(grid, x, y):
    cells = len(grid)
    fx = x / SOURCE_SIZE * cells
    fy = y / SOURCE_SIZE * cells
    x0, y0 = int(fx) % cells, int(fy) % cells
    x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
    tx, ty = fx - int(fx), fy - int(fy)
    top = grid[y0][x0] * (1 - tx) + grid[y0][x1] * tx
    bottom = grid[y1][x0] * (1 - tx) + grid[y1][x1] * tx
    return top * (1 - ty) + bottom * ty


def texture(fn, seed):
    rng = random.Random(seed)
    grain = noise_grid(rng, 16)
    coarse = noise_grid(rng, 4)
    source = bytearray(SOURCE_SIZE * SOURCE_SIZE)
    for y in range(SOURCE_SIZE):
        for x in range(SOURCE_SIZE):
            source[y * SOURCE_SIZE + x] = fn(
                x, y, rng, smooth(grain, x, y), smooth(coarse, x, y))
    # The original Tomb Explorer material contract is 64x64. Generate the art
    # at 128px so its procedural shapes stay unchanged, then take a stable
    # nearest sample. This cuts the random INDEX8 working set to one quarter.
    texels = bytearray(SIZE * SIZE)
    scale = SOURCE_SIZE // SIZE
    for y in range(SIZE):
        for x in range(SIZE):
            texels[y * SIZE + x] = source[(y * scale) * SOURCE_SIZE + x * scale]
    return bytes(texels)


def sandstone_blocks(x, y, rng, grain, coarse):
    row = y // 32
    shift = 32 if row % 2 else 0
    bx = (x + shift) % 64
    by = y % 32
    if by < 3 or bx < 3:
        return index('brown', 2 + grain * 2)
    edge = min(bx - 3, 63 - bx, by - 3, 31 - by)
    bevel = 2 if edge < 3 else 0
    chip = 1 if grain > 0.88 and edge < 8 else 0
    step = 11 + coarse * 2 + grain * 1.5 - bevel - chip
    return index('sand', step)


def floor_slabs(x, y, rng, grain, coarse):
    bx, by = x % 64, y % 64
    if bx < 3 or by < 3:
        return index('gray', 1 + grain * 2)
    crack = abs(math.sin((x + y * 0.7) * 0.18 + coarse * 6)) < 0.02
    wear = 1 if grain > 0.82 else 0
    step = 9 + coarse * 3 + grain - (4 if crack else 0) - wear
    return index('gray', step)


def rough_ceiling(x, y, rng, grain, coarse):
    pit = 2 if coarse > 0.7 and grain > 0.6 else 0
    return index('brown', 5 + coarse * 3 + grain * 2 - pit)


def carved_band(x, y, rng, grain, coarse):
    zig = abs((x % 32) - 16)
    band = abs(y - 64)
    if band < 18 and abs(zig - (y - 46) // 2) < 3:
        return index('gold', 12 + grain * 2)
    if band < 22 and abs(zig - (y - 46) // 2) < 5:
        return index('ochre', 4 + grain)
    if y < 10 or y >= 118:
        return index('brown', 3 + grain * 2)
    lotus = abs(math.sin(x * 0.2) * 10 + 64 - y) < 2 and 28 < y < 100
    if lotus:
        return index('gold', 9 + grain * 2)
    return index('ochre', 9 + coarse * 2 + grain)


def mossy_wall(x, y, rng, grain, coarse):
    base = sandstone_blocks(x, y, rng, grain, coarse)
    moss = coarse * 0.7 + grain * 0.5
    if moss > 0.68 and y > 40:
        return index('moss', 6 + grain * 5)
    return base


def stone_steps(x, y, rng, grain, coarse):
    band = y % 16
    if band < 4:
        return index('gray', 13 + grain)
    if band < 6:
        return index('gray', 3 + grain)
    return index('gray', 6 + coarse * 3 + grain)


def cloth(x, y, rng, grain, coarse):
    if 56 <= y < 72:
        return index('leather', 7 + grain * 2)
    weave = ((x // 6) + (y // 6)) % 2
    fold = math.sin(x * 0.12) * 0.6
    return index('cloth', (10 if weave == 0 else 8) + grain + fold)


def face(x, y, rng, grain, coarse):
    if y < 40:
        streak = 1 if (x + int(grain * 8)) % 11 < 3 else 0
        return index('hair', 5 + grain * 3 - streak)
    if 60 <= y < 72 and (32 <= x < 48 or 80 <= x < 96):
        return index('white', 13) if (x % 16) not in (6, 7, 8) else index('hair', 2)
    if 88 <= y < 94 and 52 <= x < 76:
        return index('red', 8)
    cheek = 1 if 48 <= y < 80 and (x < 28 or x > 100) else 0
    return index('skin', 12 + grain - cheek)


def water(x, y, rng, grain, coarse):
    wave = math.sin(x * 0.18 + coarse * 4) + math.sin(y * 0.14 + grain * 3)
    foam = 2 if wave > 1.4 else 0
    return index('water', 8 + wave * 2.2 + foam)


def bone(x, y, rng, grain, coarse):
    return index('bone', 10 + coarse * 2 + grain)


TEXTURES = [
    ('kTexWall', sandstone_blocks, 1),
    ('kTexFloor', floor_slabs, 2),
    ('kTexCeiling', rough_ceiling, 3),
    ('kTexCarved', carved_band, 4),
    ('kTexMossyWall', mossy_wall, 5),
    ('kTexSteps', stone_steps, 6),
    ('kTexCloth', cloth, 7),
    ('kTexFace', face, 8),
    ('kTexWater', water, 9),
    ('kTexBone', bone, 10),
]


def palette_bytes():
    colors = []
    for palette_index in range(256):
        colors.extend(ramp_rgb(palette_index))
    return colors


def main():
    width, height = COLUMNS * SIZE, ROWS * SIZE
    pixels = bytearray(width * height)
    for slot, (_, fn, seed) in enumerate(TEXTURES):
        col, row = slot % COLUMNS, slot // COLUMNS
        texels = texture(fn, seed)
        ox, oy = col * SIZE, row * SIZE
        for y in range(SIZE):
            start = (oy + y) * width + ox
            pixels[start:start + SIZE] = texels[y * SIZE:(y + 1) * SIZE]
    sheet = Image.frombytes('P', (width, height), bytes(pixels))
    sheet.putpalette(palette_bytes())
    destination = ROOT / 'textures.png'
    if destination.exists():
        previous = Image.open(destination)
        if (previous.mode == 'P' and previous.size == sheet.size and
                previous.tobytes() == sheet.tobytes() and
                list(previous.getpalette() or [])[:768] == palette_bytes()):
            print(f'unchanged {destination.name}')
            return
    sheet.save(destination, format='PNG', compress_level=9)
    print(f'wrote {destination.name}')


if __name__ == '__main__':
    main()
