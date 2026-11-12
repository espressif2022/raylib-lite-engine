# Shooter backdrop art — 2026-10-14

Generated with the built-in image generation tool, using the ship atlas as a
style reference. The original source image is kept so the runtime atlas can be
rebuilt without generating art again.

## Generation brief

- [Nebula source](shooter_nebula_source.jpg): premium 16-bit pixel art seen
  from above. Near-black navy base, cyan and magenta nebula across the upper
  left, a smaller violet nebula at the lower right, a small ringed planet near
  the right edge and faint asteroid silhouettes. The centre column and bottom
  quarter stay dark so ships and bullets remain readable. No text, UI, ships,
  borders, grain, dithering or bloom.

## Runtime preparation

[Backdrop preparation](prepare_nebula.py) crops the top of the square source to
the play-field aspect and reduces it to 240x195 inside one opaque 240x240 cell.
The view draws that region at exactly 2x over rows 0-389; the ground strip and
scrolling star layer are still drawn on top.
