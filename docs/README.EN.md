# Raylib Lite Engine documentation

[简体中文](README.CN.md) · [Repository README](../README.EN.md)

First use: follow the [quickstart](quickstart.EN.md) to create a game and see it on Host, then use the game development guide. The engine core is independent of a specific board; some reference games also include native device projects with explicit board dependencies.

## Guides

| Read in order | Purpose |
| --- | --- |
| [Game development](game-development.EN.md) | Organize game sources and validate a change |
| [Build paths](build-matrix.EN.md) | Choose Host, direct native, Iris native, or lobby ELF output |
| [Reusable design principles](reference-designs.EN.md) | Design platform, input, feedback, assets, and rendering |

## Reference material

| Reference | Purpose |
| --- | --- |
| [Examples](../examples/README.md) | Example and build-support matrix |
| [Host simulator (简体中文)](../host/README.md) | Host ABI, rebuild-on-change, and replay |
| [Components](../components/README.md) | Component ownership and lifecycle |
| [Audio and feedback design](audio-design.EN.md) | Events, assets, backends, and device listening |
| [New-board porting contract](board-porting.EN.md) | Video, clock, input, audio, and device acceptance |
| [Agent CLI](agent-cli.EN.md) | JSON output, exit codes, and operation scopes for finite commands |
| [Raster-kernel contract](raster-kernels.EN.md) | Texture, light, coverage, and error behavior for each `Mosaico2DDraw*` API |
| [render_benchmark](../examples/render_benchmark/README.md) | Raster benchmark and optional display preview |
| [Tests](../tests/README.md) | Host unit tests |
| [Mosaico game development skill](skills/mosaico-game-development/SKILL.EN.md) | Agent-specific workflow |

Bilingual content uses `.EN.md` and `.CN.md` suffixes. `README.md` is a language selector, while `SKILL.md` remains the tool-compatible entry point.

The public API retains the `mosaico_*` prefix for source compatibility; see [versioning](../README.EN.md#versioning). One-off measurements and migration notes belong outside the maintained `docs/` guides; local `docs/debug/` is ignored by Git.
