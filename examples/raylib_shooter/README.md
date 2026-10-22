# Mosaico Raylib Shooter

[简体中文](README.CN.md) · [Examples](../README.md)

Raylib Shooter is a vertical arcade shooter with a finite objective. Weapons fire automatically while you dodge enemy shots and break formations. The final ACE arrives after 80 seconds of active play; defeat it to win. Paused time does not advance that timer.

## Play and controls

### Device

- Tap the title screen to start.
- Each fresh touch anchors a relative drag; the ship follows finger movement without teleporting under the new contact. Release the start or retry contact before steering.
- A yellow warning ring precedes enemy fire. Heavy enemies fire three-shot spreads; the final ACE fires five. The small white marker identifies the ship’s collision core, with a short invulnerability period after damage.
- Escaping enemies do not remove lives or subtract score; they cost a scoring and combo opportunity. Consecutive kills temporarily unlock twin cannon at x4 and rapid fire at x8.
- Shots, kills, hull hits, new sectors, combo power-ups (twin cannon at x4, rapid fire at x8), the start and game over each have their own cue and haptic pulse. A space-synth loop plays during a run and ducks while paused.

### Host

- Press and drag the mouse to move the ship.
- Arrow keys or `A`/`D`/`W`/`S` move it as well.
- `Enter` starts or restarts; `P` pauses or resumes.

Touch the top-right pause button to pause and make a fresh tap to resume. After victory or defeat, release the current contact and tap `TOUCH TO RETRY`; an ongoing drag cannot restart the run.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/raylib_shooter
```

## Implementation

The game runs a 30 Hz fixed step; enemies, bullets and effects use fixed-capacity pools. Drawing uses the Raylib-compatible entry points and an RGB565 framebuffer, adapting a 480-high logical field to square and landscape surfaces. Host and device compile the same model and view sources.

The model has no event flags, so `shooter_feedback.[ch]` compares a small snapshot of counters (shots, kills, lives, sector, combo, phase) from tick to tick and plays one cue per tick, the most important first. It is tested against a stubbed audio backend. `assets_src/generate_audio.py` synthesizes the clips with numpy; the generated WAVs are not tracked in git.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection, and the [game development guide](../../docs/game-development.EN.md) for the compatibility boundary and validation workflow.
