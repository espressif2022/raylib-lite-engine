# Circuit Keep

[简体中文](README.CN.md) · [Examples](../README.md)

Circuit Keep is a 480×480 RGB565 tower defense game. Build and convert three tower types on fixed pads to stop enemies walking the road into the core on the right.

## Play and controls

### Device

1. Tap to start.
2. Pick `PULSE`, `RAPID` or `FROST` at the bottom.
3. Tap a `+` pad on the map to build.
4. With the same type selected, tap an existing tower to upgrade it; a different type converts it and refunds 50% of the previous investment.
5. The top-right button pauses or resumes; after a defeat, tap the result panel to restart.

The three towers focus on balanced damage, fire rate and slowing. Adjacent towers of different types grant combo bonuses. Start, build, shot, kill, new wave, leak and defeat have sound effects, and background music plays (quieter while paused).

### Host

Mouse clicks follow the same interface and placement rules as device touch. The browser toolbar can pause, single-step and restart.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/tower_defense
```

## Implementation

The map comes from Tiled; units and towers use RGB565+A8 atlases. The game model uses fixed pools and allocates no enemy, projectile or effect objects at runtime. Device and Host share assets, model and C drawing code.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection, and the [game development guide](../../docs/game-development.EN.md) for Host tests, replay and device acceptance.
