idf_component_register(SRCS "mosaico_game_app.c" "raylib_lite_native_hooks.c" INCLUDE_DIRS "include"
    REQUIRES mosaico_game mosaico_game_input mosaico_game_debug
    mosaico_raylib_fast mosaico_raylib_port raylib raylib_lite_platform
    raylib_lite_runner)
