# Living Worlds historical performance log

Historical experiments only. These measurements are not acceptance results for the current release; reproduce them with identified firmware and comparable board captures before making a current performance claim.

Hardware: ESP-Mosaico ESP32-S31, 480x480 RGB565, native firmware, FX LIVING.
Measurements are 50-60 second serial captures. `render` includes scene drawing;
`release` is the display handoff and is reported separately.

## 2026-09-26 experiment result

| Scene | Initial FPS | Current FPS | Initial render | Current render | Change |
| --- | ---: | ---: | ---: | ---: | ---: |
| Aurora | 24.58 | 26.23 | 40.65 ms | 38.04 ms | FPS +6.7%, render -6.4% |
| Ocean | 15.73 | 19.30 | 62.93 ms | 50.76 ms | FPS +22.7%, render -19.3% |
| Sunrise | 18.00 | 21.27 | 57.84 ms | 44.60 ms | FPS +18.2%, render -22.9% |
| Rainforest | - | 20.03 | - | 50.08 ms | Already skipped full clear |

All device runs reported zero dropped frames, busy submissions and errors.
Raw logs are under `artifacts/living-scenes-20260926/` at the repository root.

## Iterations retained

### Ocean jelly trigonometry

- Profile before: water 33-35 ms, reefs 13-19 ms, jelly about 10.6 ms.
- Change: replace fixed-step tentacle `sinf` calls with angle-addition recurrence.
- Profile after: jelly about 2.87 ms (-73%); profile-build render 64.43 to
  57.37 ms.
- Final non-profile result before removing clear: 15.73 to 17.90 FPS and
  render 62.93 to 54.02 ms.
- Kept because geometry sample count, draw order and colors are unchanged and
  the device result was stable.

### Remove redundant full-frame clear

- Aurora, Ocean and Sunrise already paint every canvas pixel through their
  background paths. Rainforest had already skipped the clear.
- Removed one 480x480 PSRAM fill: 230,400 framebuffer writes per frame.
- Device A/B:
  - Aurora: 24.58 to 26.23 FPS; 40.65 to 38.04 ms.
  - Ocean: 17.90 to 19.30 FPS; 54.02 to 50.76 ms.
  - Sunrise: 18.00 to 19.25 FPS in the pre-seed-cache A/B sample.
- Coverage validation: prefilled the host framebuffer with RGB565 `0xf81f`,
  rendered all four scenes at the default view and four extreme drag views
  per scene, and found zero untouched pixels in every case.

### Sunrise seed rotation cache

- Change: compute the seed's six rotation sine/cosine values once per seed,
  rather than once for every root, stem, husk, bend and tip point.
- Device result after redundant-clear removal: 19.25 to 21.27 FPS; render
  50.89 to 44.60 ms. Relative to the original clear-enabled baseline this is
  +18.2% FPS and -22.9% render time.
- Validation: `tests.test_living_worlds` passed and the 60-frame headless host
  render completed successfully.

## Current optimization priorities

1. Ocean RGB565 water quad sampling: about 20-22 ms in the profiled build.
   Investigate row/UV stepping and memory placement before reducing detail.
2. Ocean reef triangle rasterization: about 11-17 ms and view dependent.
   Reuse projected reef vertices between cover and visible passes.
3. Sunrise spoke trigonometry: cache or recur the fixed spoke directions and
   `sin(i*2)` values. The seed orientation cache removed the larger repeated
   cost first.
4. Rainforest panorama: 230,400 opaque scaled pixels dominate its 50 ms render.
   Inspect source/destination scaling and PSRAM bandwidth; do not add a clear.
5. Aurora background quads: now 38 ms render. Profile the RGB565 varying-v
   sampler before changing mesh density or visual effects.

## Measurement procedure

```bash
python3 tools/capture_game_perf.py artifacts/<run>/raw.log \
  --port /dev/ttyACM0 --seconds 60
python3 tools/analyze_game_perf.py --label <label> artifacts/<run>/raw.log
```

For phase diagnosis, temporarily enable
`CONFIG_MOSAICO_GAME_RASTER_PROFILE=y`; disable it for final FPS comparisons.
Never compare a profile-enabled FPS directly against a production build.
