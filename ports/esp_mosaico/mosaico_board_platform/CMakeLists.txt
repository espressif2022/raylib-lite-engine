idf_component_register(
    SRCS "mosaico_board_platform.c" "mosaico_strip_present.c"
    INCLUDE_DIRS "include"
    REQUIRES raylib_lite_platform raylib_lite_runner
             mosaico_game mosaico_game_input esp_display_present
             esp-mosaico-bsp nvs_flash esp_timer esp_driver_dma
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES freertos)
