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
idf.py -C examples/vector_puppet -B /tmp/vector-puppet-native build
```

## ESP-Mosaico native dependencies / 真机构建依赖

All standard ESP-Mosaico native Game builds use the same local dependency setup:

```sh
export MOSAICO_BSP_COMPONENT_DIR=/path/to/esp-mosaico-bsp/components/esp-mosaico-bsp
export MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils
idf.py -C examples/vector_puppet build
```

`MOSAICO_UTILS_ROOT` supplies both ESP-Iris and the upstream normal-application Recovery component. See [`examples/boards/esp-mosaico`](../boards/esp-mosaico/README.md) for the Board contract and Recovery-first device workflow.
