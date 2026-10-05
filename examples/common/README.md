# Shared native-example glue

`native_module_main.c` supplies the generic firmware entry used by standalone native game examples. `raylib_lite_example_board.h` defines the example-side board contract used by that entry, including optional haptic operations; concrete implementations live under `examples/boards/<board>/`. `native_feedback.c`/`.h` provide timed motor pulses and patterns through that contract; the game chooses which semantic events should trigger them. These files are example integration code, not a concrete BSP implementation or a mandatory dependency for Host and lobby ELF games.

Use the relevant source in a game's `main/CMakeLists.txt`. Board initialization, input sampling, codec output, and shutdown are owned by the selected board adapter; game sources must not include a concrete board API. The cross-game contract is summarized in [reusable design principles](../../docs/reference-designs.EN.md).
