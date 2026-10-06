# Game audio and feedback design

[简体中文](audio-design.CN.md) · [Design principles](reference-designs.EN.md)

The model records event meaning and a monotonically increasing sequence; the consumer handles each event exactly once. Logical asset names belong in the versioned `assets_src/game_assets.json` manifest. Cue numbers, volume, and haptic timing are game configuration. There is no shared `sfx.json` or analyzer in this repository today; do not turn an example's array into an engine standard. If games share tuning parameters, define a manifest schema, generator, and `--check` validation first.

Load and validate sounds during initialization; consume short effects and maintain music during updates; stop, unload, and release the backend on exit. Public playback APIs are in [raylib_lite_game_audio.h](../compat/raylib/include/raylib_lite_game_audio.h). The PCM backend uses a [24 kHz mono S16 contract](../include/raylib_lite/raylib_lite_audio.h) and must handle short writes, timeouts, and retryable stop. End haptic pulses on elapsed real time. [Last Zone's event mapping](../examples/last_zone_extraction/main/game_module.c) is an example, not a universal cue table.

Accept in three layers: model tests for repeated events and exactly-once consumption; Host mixer tests for samples and cleanup; device A/B listening under the same volume and scene, with codec, speaker, and firmware identity recorded. A correct Host waveform cannot establish device listening or haptic quality.
