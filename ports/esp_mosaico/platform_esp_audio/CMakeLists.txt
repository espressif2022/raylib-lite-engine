idf_component_register(
    SRCS "platform_esp_audio.c" "platform_audio_write_all.c"
    INCLUDE_DIRS "include"
    REQUIRES esp-mosaico-bsp raylib_lite_platform)
