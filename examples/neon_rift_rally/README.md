# Neon Rift Rally

[简体中文](README.CN.md) · [Examples](../README.md)

Neon Rift Rally is a third-person neon motorcycle racer. Each of three courses runs a three-lap race against six rivals using sprint, defend and chase strategies. Use drafting, nitro and drifts to fight for position.

## Play and controls

### Device

- Hold the device level during the countdown so the IMU can calibrate its center.
- Tilt the device left or right to steer; tilting left moves left and shows the left-lean pose.
- Dragging on the left side temporarily takes over steering; a second finger can hold the nitro area at the bottom right.
- During the countdown, tap either side of the course name to change course.
- Throttle is automatic; after the race, tap the result screen to restart.
- Above speed 18, holding a hard turn for about 0.27 s starts a drift; straightening out cashes it in as a short boost. Leaving the track, a collision or being airborne cancels the charge. Brief corrections do not start a drift.

### Host

- `A`/`D`: steer; `W`: throttle; `S`: brake.
- `Space`: nitro; `Shift`: drift.
- `P`: pause or resume; `Enter`: restart.
- Mouse drags on the left and the bottom-right nitro area follow the device touch rules.

## Race systems

- The bike has five lean poses (hard left, light left, center, light right, hard right) and passes through center when reversing.
- The six rivals have limited launch acceleration and a tighter starting grid. They slow behind an occupied lane and change lanes smoothly to overtake when there is room. Sprinters are faster on straights, defenders leave safe space when nearly alongside, and chasers close toward the player's lane. Each course's corners feed into rival speed decisions.
- Rival bikes and roadside sprites scale and sort by actual camera depth, grow continuously as they approach and clip naturally as they leave; helmets and back panels help tell riders apart.
- Neon Loop (harbor), Sunset Sprint (canyon) and Polar Rift (snow) have their own vistas and roadside objects; corner signs flip by direction. Ground shadows, drift sparks and a field of view that widens slightly with speed provide driving feedback.
- The harbor course follows a closed hand-authored spline through the port, a warehouse corner and a bridge. Continuous rails, evenly spaced lamp posts, warehouse fronts and bridge trusses share road coordinates; crests can hide objects behind them. Vistas on all three courses are limited to the top 184 rows and curbs are narrower, leaving room for the road and near scenery.
- Canyon sections are framed by continuous rock walls, low barriers and grouped pillars; snow sections use snowbanks, roadside fences and posts. Snow corners are gentler so the wide road does not overlap itself in tight turns.
- Following a rider closely and aligned builds draft; a full meter grants a short speed bonus.
- During a race the HUD shows only position and nitro. Gap to the rider ahead, the draft meter, position changes and the last-lap prompt are not drawn; draft and nitro are still handled by the model and audio. OFF TRACK appears when you leave the track. The results screen is kept.
- Collisions, lap completion, overtaking, being overtaken and drafting each have layered sound and haptics.

## Host preview

From the repository root:

```sh
python3 tools/game_cli.py sim examples/neon_rift_rally
```

## Implementation

Host and device share the 30 Hz fixed-step model, procedural corners and RGB565 view. The game uses fixed-capacity state, a single bike-pose texture and event-driven feedback; the device stores only the best lap and race times for each of the three courses.

## Native build

See [build paths](../../docs/build-matrix.EN.md) for targets and Board selection. Development records such as steering tuning, Host measurements and two-board comparisons live in the repository under `docs/development/neon-rift-rally/` and are not shipped in the release package. See the [game development guide](../../docs/game-development.EN.md) for tests, replay and device acceptance.
