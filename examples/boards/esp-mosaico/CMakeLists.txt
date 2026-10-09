idf_component_register(
    SRCS
        "board.c"
        "mosaico_strip_present.c"
        "board_audio_codec.c"
    PRIV_INCLUDE_DIRS
        "."
    REQUIRES
        examples_common
        examples_audio
        raylib-lite-engine
        esp_display_present
        esp-mosaico-bsp
        esp_system
        heap
        nvs_flash
        esp_timer
        esp_driver_dma
    PRIV_REQUIRES freertos
    # Retain the selected Board's audio backend factory at final link.
    WHOLE_ARCHIVE)
