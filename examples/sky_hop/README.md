# Sky Hop

[简体中文](README.CN.md) · [Examples](../README.md)

Sky Hop is a four-level side-scrolling platformer. Collect coins, stomp patrolling enemies and reach the end of each level; lives, score and coin-chain bonuses carry across levels, and clearing the fourth level finishes the run.

## Play and controls

### Device

- Tap the title card to start.
- The bottom `<` and `>` buttons move left and right, `JUMP` jumps; short and long presses give different jump heights.
- Movement and jump can be held together. The top-right button pauses and resumes.
- Button, joystick and IMU input map to the same game actions.
- Jump, coin, stomp, hurt and level clear have sound effects, and music plays during a level.

### Host

- `A`/`D` or arrow keys: move.
- `Space`: jump.
- `Enter`: start or go to the next level.
- `P`: pause or resume.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/sky_hop
```

Host and device share the gameplay model, Camera2D world view and RGB565 drawing code; audio, physical input and LCD output still need device verification.

## Implementation

The game uses fixed-capacity objects, four continuous levels, variable jump height, coin-chain bonuses, scene slide-ins and particles. The best score is kept only for the current session and is not saved. Native firmware embeds the assets in the application image.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets, Board selection and standalone examples, and the [game development guide](../../docs/game-development.EN.md) for Host tests, fixed replay and device acceptance.
