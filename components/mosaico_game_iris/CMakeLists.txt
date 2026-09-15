idf_component_register(
    SRCS "mosaico_game_iris.c"
    INCLUDE_DIRS "include"
    REQUIRES mosaico_game esp_iris esp_partition mbedtls spi_flash heap
)
