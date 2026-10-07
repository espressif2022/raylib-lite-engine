set(RAYLIB_LITE_EXAMPLE_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common")
idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
    raylib-lite-engine COMPONENT_DIR)
file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")

idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}/native_module_main.c"
         "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}/native_feedback.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}"
    REQUIRES ${RAYLIB_LITE_BOARD} esp_timer)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    RAYLIB_LITE_GAME_NATIVE=1 RAYLIB_LITE_NATIVE_TARGET_FPS=30)

include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
