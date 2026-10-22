# Circuit Keep

[简体中文](README.CN.md) · [Examples](../README.md)

Circuit Keep is a six-wave RGB565 tower defense game for square and landscape surfaces. Build and convert three tower types on nine fixed pads to stop enemies reaching the core on the right. Survive the sixth wave with core health remaining to win.

## Play and controls

### Device

1. Tap to enter preparation. There is no preparation deadline and enemies do not spawn automatically.
2. Pick `PULSE`, `RAPID` or `FROST` at the bottom.
3. Tap a `+` pad to preview its range, then tap `BUILD` to confirm or `CANCEL` to dismiss.
4. Tap an existing tower to inspect it. Confirm `UPGRADE` for the same type, or `REPLACE` for a different type. The confirmation shows the net debit and applied refund (50% of previous investment, capped at the new tower price).
5. When ready, dismiss the selected pad and tap the upper `START WAVE` control, which also lists the enemy count. Spawning begins after a short one-second countdown. Each completed wave returns to preparation so you can build or upgrade before manually starting the next.
6. The top-right button pauses or resumes; after victory or defeat, tap the result panel to restart.

The three towers focus on balanced damage, fire rate and slowing. PULSE deals double damage to a FROST-slowed target; RAPID deals 2 extra damage. This combination does not require adjacent towers. Start, build, shot, kill, new wave, leak and defeat have sound effects, and background music plays, quieter while paused and stopped after victory or defeat.

### Host

Mouse clicks follow the same interface and placement rules as device touch. `Enter` starts a new run from the title or result screen; `P` pauses or resumes. Each wave still requires the `START WAVE` control. The browser toolbar can pause, single-step and restart.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/tower_defense
```

## Implementation

The map comes from Tiled; units and towers use RGB565+A8 atlases. The game model uses fixed pools and allocates no enemy, projectile or effect objects at runtime. Device and Host share assets, model and C drawing code.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection, and the [game development guide](../../docs/game-development.EN.md) for Host tests, replay and device acceptance.
