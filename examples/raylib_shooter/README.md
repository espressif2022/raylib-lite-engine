# Mosaico Raylib Shooter

[简体中文](README.CN.md) · [Examples](../README.md)

Raylib Shooter is a vertical arcade shooter reference game. Drag the ship to dodge bullet patterns while the weapon fires automatically; breaking formations and elite enemies in a row earns bonuses.

## Play and controls

### Device

- Tap the title screen to start.
- Drag the ship on screen; firing is automatic.
- The shared game module handles touch and pause state; the game has no sound effects yet.

### Host

- Press and drag the mouse to move the ship.
- Arrow keys or `A`/`D`/`W`/`S` move it as well.
- `Enter` starts or restarts; `P` pauses or resumes.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/raylib_shooter
```

## Implementation

The game runs a 30 Hz fixed step; enemies, bullets and effects use fixed-capacity pools. Drawing uses the Raylib-compatible entry points but writes straight into a 480×480 RGB565 framebuffer. Host and device compile the same model and view sources.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection, and the [game development guide](../../docs/game-development.EN.md) for the compatibility boundary and validation workflow.
