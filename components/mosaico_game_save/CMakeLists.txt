idf_component_register(
    SRCS "mosaico_game_save.c" "mosaico_game_save_nvs.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash)
