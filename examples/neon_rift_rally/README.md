# Neon Rift Rally

`Neon Rift Rally` is a third-person panoramic hover-racing game for the
Mosaico Raylib Lite engine. It combines a procedural curved track and layered
panorama with a compact formal craft atlas, three deterministic rivals,
ranking, collision/near-miss scoring, integrity, three-lap results, persistent
records, synthesized audio, and event-driven haptics.

《Neon Rift Rally》是第三人称全景悬浮赛车。第一阶段采用程序化赛道、霓虹地平线、
悬浮车、检查门和三圈计时，不依赖贴图资产，先建立 Host/真机共用的 3D 全景与光栅
性能基线，再逐步加入赛道主题、音效和幽灵车。

## Controls / 操作

- Host keyboard: `A`/`D` steer, `W` throttle, `S` brake, `Space` nitro,
  `Shift` drift.
- Touch: drag from the lower-left steering pad; push upward to accelerate. A
  second finger can hold the lower-right nitro area at the same time.
- `P` pauses/resumes in the Host shell. `Enter`/action `4` restarts the run;
  tapping the finished race also starts a fresh run. The runner reset control
  performs the same restart and clears transient touch state.
- During the countdown, action `8`/`9` (or a tap in the upper-left/upper-right
  corners) selects the previous/next course. Selection resets the countdown.

The three course cards are `Neon Loop / NEON GRID`, `Sunset Sprint / SUNSET
EMBER`, and `Polar Rift / AURORA ICE`. They share the deterministic track
model but start at different sections of the closed panoramic route, so each
has a distinct visual opening and independent best records.

The module uses action codes `0`, `1`, `2`, `5`, `6`, and `7` for left, right,
throttle, brake, nitro, and drift; action `3` toggles pause and action `4`
restarts. Replays can therefore describe the same controls without knowing the
rendering implementation.

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
Last Zone and Tomb Explorer. Configure the Mosaico component and BSP paths if
they are not adjacent to this checkout, then run `idf.py build` from this
directory. `main/CMakeLists.txt` embeds the generated assets directory. The
current firmware embeds nine compact `.sound` resources. Semantic events
map to cues and haptics as follows: nitro, drift, jump, landing, checkpoint,
lap, finish, and off-track. Missing audio assets are tolerated at runtime, so
the model and event serial remain usable on products without audio.

Build and flash:

```bash
source /home/xx/work/mosaico/esp-idf-pinned/export.sh
idf.py -B build build
idf.py -B build -p /dev/ttyACM0 flash
```

## Interface contract for the gameplay/view implementation

`main/game_module.c` intentionally assumes this small C API; the model/view
implementation is left separate so it can be tested and iterated independently:

```c
void rally_reset(rally_game_t *game);
void rally_set_controls(rally_game_t *game, float throttle, float brake,
                        float steer, bool drift, bool nitro);
void rally_update(rally_game_t *game);
uint32_t rally_state_hash(const rally_game_t *game);
int rally_view_render(const rally_game_t *game, MosaicoAtlas rally_art);
```

The deterministic model owns track-local motion, jumping, checkpoints, laps,
nitro and semantic event flags. The renderer samples the exact same procedural
track function, so collision and perspective geometry cannot drift apart.

For repeatable device measurements, record the firmware hash, display
configuration, fixed input sequence, and raw logs alongside each result.
