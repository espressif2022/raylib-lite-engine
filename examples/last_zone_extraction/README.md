# Last Zone: Extraction

[简体中文](README.CN.md) · [Examples](../README.md)

Last Zone is a lightweight tactical shooter. Complete five training missions in order: search for supplies, clear enemies and reach the extraction point. Later missions give less ammunition and armor and more elite enemies.

![Last Zone: Extraction](docs/screenshot.png)

The image is the Host RGB565 frame of the Dock mission.

## Play and controls

### Device

- Tap the briefing to deploy or redeploy.
- The left stick moves; push to the outer ring ahead to sprint.
- Drag on the right side to turn and pitch.
- Tap the fire button to shoot or open the door you face; hold fire to steady the weapon.
- The radar panel can be dragged out of the way.

### Host

- `W`/`S`: forward and back; `Q`/`E`: strafe.
- `A`/`D`: turn; `Shift`: sprint.
- `F` or `Ctrl`: fire or interact.
- Drag on the right side with the mouse: adjust the view.

## Campaign

| Mission | Focus | Enemies |
| --- | --- | ---: |
| Dock | Observation tutorial | 5 |
| Depot | Area control | 7 |
| Command | Flank breakthrough | 8, including 2 elites |
| Ghost | Ambush | 8 |
| Run | Assault | 9, including 3 elites |

Device NVS stores campaign progress and each mission's best result.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/last_zone_extraction
```

## Implementation

Host and device share the fixed-step model, perspective scene and RGB565 view. Gameplay includes cover, window openings, doors, ammunition, armor, elite enemies and extraction checks; sound and haptics are driven by semantic events.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection. Flashing a standalone project replaces the device's current firmware; see the [game development guide](../../docs/game-development.EN.md) for tests and device acceptance.
