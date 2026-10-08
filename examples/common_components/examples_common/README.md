# Shared example Application component

`examples_common` owns the board-neutral Application/example glue shared by repository native examples. It is an application-side IDF component, not part of the Engine public API.

- `project.cmake`: pre-`project()` Application composition; selects `RAYLIB_LITE_BOARD`, registers this component plus the selected `examples/boards/<board>` component, and loads Board defaults/configuration.
- `native_module_main.c`: generic native example launcher.
- `include/raylib_lite_example_board.h`: abstract Board contract implemented by the selected Board component.
- `native_feedback.c` and `include/native_feedback.h`: generic haptic timing helper over that Board contract.
- `include/raylib_lite_game_module.h`: neutral Host/native module contract used only by examples and Host adapters.
- `include/raylib_lite_game_module_contract.h`: example/application bridge used by shared Game sources; Product ELF ABI mapping stays outside the Engine component.

Concrete Board implementations remain under `examples/boards/<board>/`. Game-specific device-only implementation stays inside that Game's `main/` component, for example `main/native/`; there is no Board×Game extension layer.
