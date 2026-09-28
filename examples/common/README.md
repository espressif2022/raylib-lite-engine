# Shared native-example glue

`native_module_main.c` supplies the generic firmware entry used by standalone native game examples. `native_feedback.c`/`.h` provide timed motor pulses and patterns; the game chooses which semantic events should trigger them. These files are example integration code, not the board/BSP implementation or a mandatory dependency for Host and lobby ELF games.

Use the relevant source in a game's `main/CMakeLists.txt`, and keep board initialization, input sampling, codec output, and shutdown ownership explicit. The cross-game contract is summarized in [reusable design principles](../../docs/reference-designs.EN.md).
