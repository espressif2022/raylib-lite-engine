idf_component_register(
    SRCS "mosaico_game_assets.c" "mosaico_game_assets_mmap.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES esp_mmap_assets)
