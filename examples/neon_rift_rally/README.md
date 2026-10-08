# Neon Rift Rally

`Neon Rift Rally` is a third-person neon motorcycle racing game for the
Mosaico Raylib Lite engine. It combines a procedural curved track and layered
panorama with three rider poses, six deterministic rivals,
ranking, collision/near-miss scoring, integrity, three-lap results, persistent
records, synthesized audio, and event-driven haptics.

《Neon Rift Rally》是第三人称霓虹摩托竞速游戏。赛道、车道线和路边物件按游戏进度
投影；骑手按转向切换左倾、直行、右倾姿态。碰撞会减速、扣耐久，并触发画面、音效和
事件反馈。Host 与真机共用游戏模型和视图源码。

## Controls / 操作

- Host keyboard: `A`/`D` steer, `W` throttle, `S` brake, `Space` nitro,
  `Shift` drift.
- Native board: acceleration is automatic. Tilt the board left/right to steer;
  keep it level during the opening countdown so the IMU can calibrate. A
  deliberate tilt beyond the deadzone changes lanes. A left-side touch drag
  overrides tilt steering; a second finger can hold the lower-right nitro
  area. Host pointer steering also requests throttle.
- IMU steering uses the calibrated gravity-vector roll angle, with an
  approximately 6-degree deadzone and finer response near center. Return the
  board to its calibrated pose to release steering. Obvious acceleration
  spikes are rejected; sustained steering still moves toward the road edge.
- Touch steering starts neutral at the initial contact. Drag horizontally
  from that point; resting a finger on the left edge does not apply full lock.
- `P` pauses/resumes in the Host shell. `Enter`/action `4` restarts the run;
  tapping the finished race also starts a fresh run. The runner reset control
  performs the same restart and clears transient touch state.
- During the countdown, action `8`/`9` (or a tap beside the course name on
  the left/right) selects the previous/next course. Selection resets the countdown.

The three course cards are `Neon Loop / NEON GRID`, `Sunset Sprint / SUNSET
EMBER`, and `Polar Rift / AURORA ICE`. Each uses a distinct closed route and
color palette, with independent best records.

The module uses action codes `0`, `1`, `2`, `5`, `6`, and `7` for left, right,
throttle, brake, nitro, and drift; action `3` toggles pause and action `4`
restarts. Replays can therefore describe the same controls without knowing the
rendering implementation.

The shared IMU input uses the Board adapter's X-axis acceleration. The game
filters a 0.12 g deadzone and recenters when samples stop; touch and action
steering take priority. Check the physical left/right sign on the target board
before accepting this control as device-ready.

Each completed lap records its fixed-step duration. Host metadata exposes
`last_lap_ticks`, `best_lap_ticks`, and `best_race_ticks` for replay/evaluation;
native firmware stores the two best values in NVS under the
`neon_rift_rally/record` key. Host runs intentionally keep records in memory so
replays remain isolated and deterministic.

The countdown metadata exposes a short start instruction (`select_course`,
`throttle_ready`, `hold_drift`, or `nitro_go`). A finished race reports a
`gold`, `silver`, or `bronze` rating/trophy from the final position; a failed
run reports `dnf` with no trophy.

## Host simulation / Host 仿真

```bash
python3 tools/game_cli.py sim examples/neon_rift_rally
python3 tools/game_cli.py sim examples/neon_rift_rally --headless --frames 120
examples/neon_rift_rally/tests/run_host_tests.sh
```

The Host runner compiles `game.sim.json` and the
`main/rally_game.c`/`main/rally_view.c` sources into one shared module. Audio
is regenerated deterministically with `assets_src/generate_audio.py`.

The two-finger regression replay is `tests/two_finger_drive.json`; run both the
model and Host pointer checks with `tests/run_host_tests.sh`.

## Native project / 真机工程

The example root is a direct ESP-IDF project using the same native structure as
Last Zone and Tomb Raycast. The selected Board automatically fetches pinned dependencies. Run
`idf.py build` from this directory with the supported ESP-IDF environment active. `main/CMakeLists.txt` embeds the generated assets directory. The
current firmware embeds nine compact `.sound` resources. Semantic events
map to cues and haptics as follows: nitro, drift, jump, landing, checkpoint,
lap, finish, and off-track. Missing audio assets are tolerated at runtime, so
the model and event serial remain usable on products without audio.

Build from the repository root:

```sh
idf.py --preview -C examples/neon_rift_rally -B /tmp/neon-rift-rally-native -DIDF_TARGET=esp32s31 build
```

Deployment uses the [Board workflow](../boards/esp-mosaico/README.md). Iris USB services and log redirection are disabled by default; the console uses UART0. The current example supports Host and native firmware.

## Interface contract for the gameplay/view implementation

`main/game_module.c` intentionally assumes this small C API; the model/view
implementation is left separate so it can be tested and iterated independently:

```c
void rally_reset(rally_game_t *game);
void rally_set_controls(rally_game_t *game, float throttle, float brake,
                        float steer, bool drift, bool nitro);
void rally_update(rally_game_t *game);
uint32_t rally_state_hash(const rally_game_t *game);
int rally_view_render(const rally_game_t *game, raylib_lite_atlas_t rally_art);
```

The deterministic model owns track-local motion, jumping, checkpoints, laps,
nitro and semantic event flags. The renderer samples the exact same procedural
track function, so collision and perspective geometry cannot drift apart.

For repeatable device measurements, record the firmware hash, display
configuration, fixed input sequence, and raw logs alongside each result.
The current build has no board FPS capture, so Host render time is not a device
frame-rate result. After an authorized flash, replay the same steering, nitro,
collision, and course-selection sequence on the target board. Capture startup,
input, present, shutdown, and a 60-second frame log with:

```bash
python3 tools/capture_game_perf.py artifacts/neon-rift-rally/raw.log \
  --port <UART_SERIAL_PORT> --seconds 60
python3 tools/analyze_game_perf.py --label neon-rift-rally \
  artifacts/neon-rift-rally/raw.log
```

Check the full-frame distribution and dropped presents against the 30 Hz
33.3 ms frame budget, then inspect the same moments on the display. Record
rendering, waiting for buffers, and presentation separately.

The [Host profiling report](performance/host-optimization.md) records the
fixed-state quality comparison, phase measurements and renderer optimizations.
The [steering tuning report](performance/steering-tuning.md) records the IMU
pulse, repeated swing and return-to-center checks.

## ESP-Mosaico native dependencies / 真机构建依赖

Standard ESP-Mosaico native Game builds automatically download pinned Git dependencies.
No BSP or utilities environment exports are required. See the [Board guide](../boards/esp-mosaico/README.md).
