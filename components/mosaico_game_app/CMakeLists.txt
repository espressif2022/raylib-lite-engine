idf_component_register(
    SRCS "mosaico_game_app.c"
    INCLUDE_DIRS "include"
    REQUIRES mosaico_game mosaico_game_input mosaico_game_debug
             mosaico_raylib_fast mosaico_raylib_port esp-gsp esp-mosaico-bsp
             nvs_flash esp_timer
)
