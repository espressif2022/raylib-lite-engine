idf_component_register(
    SRCS "board.c" "box3_video.c"
    PRIV_INCLUDE_DIRS "."
    REQUIRES examples_common raylib-lite-engine esp_board_manager esp_lcd esp_lcd_touch esp_timer heap
    PRIV_REQUIRES freertos)
