get_filename_component(_audio_core
    "${CMAKE_CURRENT_LIST_DIR}/../../../components/mosaico_game_audio" ABSOLUTE)
idf_component_register(
    SRCS
        "mosaico_game_audio.c"
        "${_audio_core}/raylib_lite_audio_decode.c"
        "${_audio_core}/raylib_lite_audio_mixer.c"
    INCLUDE_DIRS "${_audio_core}/include"
    PRIV_INCLUDE_DIRS "${_audio_core}/private"
    REQUIRES mosaico_game_assets platform_esp_audio raylib raylib_lite_platform)
