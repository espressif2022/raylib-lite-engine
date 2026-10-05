# Vector Puppet

A parameter-driven vector character in a night classroom. Everything is drawn
from code at runtime: no bitmaps or packed assets.

- `vg_raster.c`: non-zero path fill with four sub-scanlines. Interior pixels
  are plain runs; only span ends are blended, so anti-aliasing cost follows the
  outline length.
- `puppet_rig.c`: named parameters driven by layered sources. Keyed actions
  fade over an idle base, then look-at, tilt, blink, and hair springs.
- `puppet_draw.c`: the character reads only parameters. Head yaw and pitch
  shift layers by depth for a 2.5D turn.

Tap to play the next action and drag to steer the gaze. After five idle
seconds the actions cycle automatically. The HUD shows vector render time,
fill count, and solid and anti-aliased pixel counts.

```sh
python3 tools/game_cli.py sim examples/vector_puppet
export MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp
idf.py -C examples/vector_puppet -D RAYLIB_LITE_BOARD=esp-mosaico \
    -B /tmp/vector-puppet-native build
```
