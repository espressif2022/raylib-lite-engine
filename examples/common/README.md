# Shared native example glue

This directory contains board-neutral Application/example glue shared by the repository examples. It is not part of the Engine public API; package-content filtering is currently deferred.

- `raylib_lite_example_project.cmake`: Application-side Board selector; defaults to `esp-mosaico`, adds only the selected `examples/boards/<board>` component, and loads Board pre-project defaults without introducing Board knowledge into the Engine. Game-specific device adapters stay with the Game under `examples/<game>/native/`; there is no Board×Game extension layer.
- `native_module_main.c`: generic native example launcher.
- `raylib_lite_example_board.h`: abstract Board contract used by the launcher.
- `native_feedback.[ch]`: generic haptic timing helper over that Board contract.
- `raylib_lite_game_module.h`: neutral Host/native module contract used only by examples and Host adapters; it is not an Engine public API.
- `raylib_lite_game_module_contract.h`: example/application bridge used by shared Game sources; Product ELF ABI mapping stays outside the Engine component.

Concrete Board implementations remain under `examples/boards/<board>/`.
