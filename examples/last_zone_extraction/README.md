# Last Zone: Extraction

[简体中文](README.CN.md) · [Examples](../README.md)

Last Zone is a lightweight tactical shooter. Complete five training missions in order: search for supplies, collect the `DATA` case and reach extraction. Enemies may remain alive; an undetected, undamaged extraction earns an S grade. Later missions give less ammunition and armor and more elite enemies.

![Last Zone: Extraction](docs/screenshot.png)

The image is the Host RGB565 frame of the Dock mission.

## Play and controls

### Device

- Tap the briefing to deploy or redeploy.
- The left stick moves; push to the outer ring ahead to sprint.
- Hold and drag on the right to aim; release to fire. The left finger can keep moving, so moving, aiming and firing need only two contacts. Holding still steadies the weapon.
- Face a nearby door and tap the central `OPEN` button. Firing never opens doors.
- The radar panel can be dragged out of the way.

### Host

- `W`/`S`: forward and back; `Q`/`E`: strafe.
- `A`/`D`: turn; `Shift`: sprint.
- `F` or `Ctrl`: hold to steady, release to fire.
- Drag on the right with the mouse to aim and release to fire; click `OPEN` for doors. Replay action `10` invokes door interaction.

## Campaign

| Mission | Focus | Enemies |
| --- | --- | ---: |
| Dock | Observation tutorial | 5 |
| Depot | Area control | 7 |
| Command | Flank breakthrough | 8, including 2 elites |
| Ghost | Ambush | 8 |
| Run | Assault | 9, including 3 elites |

Device NVS keeps the existing save format, campaign progress and best times. Historical times earned under the clear-all rule remain stored and are not directly comparable to the new retrieval route.

## Hostiles

- Three roles, told apart by the shoulder flash: riflemen (blue) hold mid range behind cover, flankers (amber) move faster and pick spots outside your view, and heavies (red, the elites) take three hits, press in and barely flinch.
- Patrols walk their own routes around their post, pause and look around. They see in a 120 degree cone, so you can slip past behind them. A glimpse builds suspicion (an amber bar and "?" over the head) rather than an instant alert; standing still far away is hard to spot, sprinting in the open is not. A patrol that loses a half-seen shape walks over to check. Sprinting and gunfire are still heard all around.
- An alerted hostile radios patrols within about seven cells after a short delay (an antenna icon over its head, then an "ENEMY RADIO" banner). Kill the caller first to cut the call for a +50 bonus.
- Enemy fire is a roll: moving makes you harder to hit, sprinting much harder, and close range easier. Misses show a tracer and a yellow arrow toward the shooter, with a whizz cue.
- A hit staggers the target and interrupts its aim. A wounded rifleman or flanker falls back to cover; heavies keep coming. Searchers sweep nearby cells around the last sighting instead of standing still.
- Radar blips show which way each hostile faces; calm patrols are dim, suspicious ones amber.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/last_zone_extraction
```

## Implementation

Host and device share the fixed-step model, perspective scene and RGB565 view. Gameplay includes cover, window openings, doors, ammunition, armor, elite enemies and extraction checks; sound and haptics are driven by semantic events.

- The crosshair opens with movement, sprint and recoil, and tightens while you hold fire to steady. It turns red when the magazine is empty.
- The radar is translucent, with a sweep arm and pulsing hostile blips; its frame flashes when an enemy is alerted.
- Kills show a `TARGET DOWN` banner with the kill count. Pickups, gates, empty magazines and gate hints use backed labels that stay readable over bright sky.
- A tense stealth loop plays while you scout. When any enemy is alerted or engaging, a driving combat loop takes over and holds for about five seconds after contact ends. Gates, deploying and death each have their own cue, and music ducks while paused.
- `assets_src/generate_audio.py` synthesizes every clip; the generated WAVs are not tracked in git. The debug FPS label is compiled only with `LAST_ZONE_SHOW_FPS`.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection. Flashing a standalone project replaces the device's current firmware; see the [game development guide](../../docs/game-development.EN.md) for tests and device acceptance.
