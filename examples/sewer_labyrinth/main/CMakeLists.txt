file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_module_main.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_ENGINE_ROOT}/examples/common"
    REQUIRES ${RAYLIB_LITE_BOARD_COMPONENT} mosaico_game_app mosaico_game_2d
             mosaico_raylib_fast mosaico_raylib_port raylib)
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${RAYLIB_LITE_ENGINE_ROOT}/host/include")
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_GAME_NATIVE=1)
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
