---
name: mosaico-game-development
description: Create, extend, debug, test, package, or install Raylib Lite Engine games, including 2D sprites, raycast worlds, portal rooms, and volume meshes.
---

# Mosaico Game Development

Raylib Lite Engine retains the `mosaico_*` public API prefix for source compatibility; see [versioning](../../../README.EN.md#versioning). The optional Iris native project is distinct from a lobby ELF game.

Read the [game development guide](../../game-development.EN.md), [build paths](../../build-matrix.EN.md), and [reusable design principles](../../reference-designs.EN.md). Choose a reference game using the [example support matrix](../../../examples/README.md), which includes `neon_rift_rally` and the dedicated `render_benchmark` project. The public headers linked by the design guide are the API source of truth.

## Agent-specific checks

1. Keep gameplay state, logic, and shared drawing portable. Check generated assets and gameplay input in the Host simulator before claiming device behavior.
2. Run focused model checks with `-Wall -Wextra -Werror`, relevant Host tests, and a fixed-input headless replay. Record exactly which checks passed.
3. For device work, name the tested path: **direct native firmware**, **Iris native firmware**, or **lobby ELF game**. A Host result does not prove display, audio, haptics, or board input.
4. For a lobby ELF game, use the external `esp-mosaico-elf-game-sdk` package and a compatible lobby firmware; for native firmware, pass board/BSP paths explicitly. Preserve separate build directories and record firmware identity, configuration, and device logs.

The engine does not own production board or flash-partition policy. Refer to [components](../../../components/README.md) for component ownership. [简体中文](SKILL.CN.md)
