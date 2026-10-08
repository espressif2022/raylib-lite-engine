idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
    raylib-lite-engine COMPONENT_DIR)
file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources}
    INCLUDE_DIRS "."
    REQUIRES examples_common ${RAYLIB_LITE_BOARD} esp_timer)
target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
set_source_files_properties(tomb_view.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
