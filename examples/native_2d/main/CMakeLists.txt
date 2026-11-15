idf_component_register(
    SRCS "reference_game.c" "native/board_main.c"
    PRIV_INCLUDE_DIRS "."
    REQUIRES examples_common examples_audio ${RAYLIB_LITE_BOARD} log
)

if(TARGET idf::espressif2022__raylib-lite-engine)
    idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
        espressif2022__raylib-lite-engine COMPONENT_DIR)
else()
    idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
        raylib-lite-engine COMPONENT_DIR)
endif()
include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/.."
    EXTRA_FILES sprite.png glyphs.png glyphs.fnt)
