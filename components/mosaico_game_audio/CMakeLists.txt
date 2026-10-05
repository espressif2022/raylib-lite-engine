idf_component_register(
    SRCS "raylib_lite_audio_decode.c" "raylib_lite_audio_mixer.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES raylib_lite_platform raylib)
