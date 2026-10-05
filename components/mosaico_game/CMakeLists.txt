idf_component_register(SRCS "mosaico_game.c" INCLUDE_DIRS "include"
    REQUIRES esp_timer esp_system heap raylib_lite_runner PRIV_REQUIRES freertos)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MOSAICO_GAME_DEFAULT_FPS=${CONFIG_MOSAICO_GAME_DEFAULT_FPS})
