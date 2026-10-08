# Raylib Lite Engine documentation

[简体中文](README.CN.md) · [Repository README](../README.md)

First use: follow the [quickstart](quickstart.EN.md) to create a game and see it on Host, then use the game development guide. The engine core is independent of a specific board; some reference games also include native device projects with explicit board dependencies.

## Guides

| Read in order | Purpose |
| --- | --- |
| [Game development](game-development.EN.md) | Organize game sources and validate a change |
| [Build paths](build-matrix.EN.md) | Choose Host, generic native, or ELF integration; Vibe owns Iris product workflows |
| [Reusable design principles](reference-designs.EN.md) | Design platform, input, feedback, assets, and rendering |

## Reference material

| Reference | Purpose |
| --- | --- |
| [Public API contract](../API.md) | Lifecycle, ownership, input and backend contracts |
| [Component release](releasing.md) | Assemble independent examples, validate packages and publish from main |
| [Asset provenance](../release/asset_provenance.json) | Maintained example media origins |
| [Examples](../examples/README.md) | Example and build-support matrix |
| [Host simulator (简体中文)](../host/README.md) | Host ABI, rebuild-on-change, and replay |
| [Engine capabilities](engine-capabilities.EN.md) | Single-component API catalog and internal module ownership |
| [Audio and feedback design](audio-design.EN.md) | Events, assets, backends, and device listening |
| [New-board porting contract](board-porting.EN.md) | Video, clock, input, audio, and device acceptance |
| [Agent CLI](agent-cli.EN.md) | JSON output, exit codes, and operation scopes for finite commands |
| [Raster-kernel contract](raster-kernels.EN.md) | Texture, light, coverage, and error behavior for each `raylib_lite_2d_draw_*` API |
| [render_benchmark](../examples/render_benchmark/README.md) | Raster benchmark and optional display preview |
| [Tests](../tests/README.md) | Host unit tests |
| [Mosaico game development skill](skills/mosaico-game-development/SKILL.EN.md) | Agent-specific workflow |

Repository and documentation indexes use `README.md` for English and `README.CN.md` for Chinese. Contribution and detailed bilingual guides use `.EN.md` and `.CN.md`. `SKILL.md` remains the tool-compatible entry point. Single-language references are labelled in this index.

Public Engine APIs use the `raylib_lite_*` namespace; Raylib-shaped compatibility lives under `compat/raylib/`. See [versioning](../README.md#versioning). One-off measurements and migration notes belong outside the maintained `docs/` guides; local `docs/debug/` is ignored by Git.
