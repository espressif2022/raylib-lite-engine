# Rally environment art — 2026-10-13

Generated with the built-in image generation tool. The original source images
are retained so the runtime atlas can be rebuilt without generating art again.
Existing motorcycle poses are reused.

## Generation briefs

- [Panorama source](rally_panoramas_source.png): premium late-1990s arcade pixel
  art, three stacked horizontal environments. Neon coastal city with cyan and
  magenta buildings and moonlight; orange sunset canyon with layered plum rocks;
  polar fjord with blue mountains and aurora. Sky and horizon only, open central
  sightline, no road, vehicles, UI, text or borders. Clean pixel clusters,
  restrained palette, no grain, bloom or dithering.
- [Prop source](rally_props_source.png): four independent pixel-art objects on
  a transparent 2×2 sheet. Dark concrete neon barrier, low ochre canyon rock,
  snow-covered blue rock, and a short double-right-chevron sign with gold arrows.
  Opaque object pixels, no baked ground shadow, UI, glow or extra objects.

## Runtime preparation

[Sky preparation](prepare_track_background.py) crops the three equal source
panels into 224×224 cells. The view expands each cell to a 480×224 panorama,
preserving the original horizontal composition. The atlas is opaque RGB565.

[Prop preparation](prepare_rally_sprites.py) uses explicit crop regions because
the snow silhouette crosses the source cell boundary. It bounds and bottom-aligns
pixels that survive binary-alpha packing, removing faint transparent fringes
that otherwise make a rock appear to float. Ground shadows are drawn separately.
The runtime has four 96×96 prop frames; the previous ten unused frames are no
longer packed. Frame names are declared in [the manifest](game_assets.json).

The motorcycle's five authored lean poses supply rotation without the general
affine sampler. Rival identity uses a small opaque color patch instead of tinting
every sprite pixel. No full-screen translucent effects are added.
