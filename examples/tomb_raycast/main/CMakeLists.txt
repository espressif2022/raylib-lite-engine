file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_module_main.c"
         "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_feedback.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_ENGINE_ROOT}/examples/common"
    REQUIRES raylib_lite_engine esp_timer)
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${RAYLIB_LITE_ENGINE_ROOT}/host/include")
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_GAME_NATIVE=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_NATIVE_TARGET_FPS=50)
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
set_source_files_properties(tomb_view.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
