get_filename_component(RAYLIB_LITE_ENGINE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(_audio_core "${RAYLIB_LITE_ENGINE_ROOT}/components/mosaico_game_audio")

idf_component_register(
    SRCS
        "board.c"
        "mosaico_strip_present.c"
        "mosaico_game_audio.c"
        "platform_esp_audio.c"
        "platform_audio_write_all.c"
        "${_audio_core}/raylib_lite_audio_decode.c"
        "${_audio_core}/raylib_lite_audio_mixer.c"
    INCLUDE_DIRS "${_audio_core}/include"
    PRIV_INCLUDE_DIRS
        "."
        "${_audio_core}/private"
        "${RAYLIB_LITE_ENGINE_ROOT}/examples/common"
    REQUIRES
        raylib_lite_platform
        raylib_lite_runner
        mosaico_game_input
        mosaico_game_assets
        raylib
        esp_display_present
        esp-mosaico-bsp
        nvs_flash
        esp_timer
        esp_driver_dma
    PRIV_REQUIRES freertos)
