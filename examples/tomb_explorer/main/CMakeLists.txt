file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_module_main.c"
         "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_feedback.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_ENGINE_ROOT}/examples/common"
    REQUIRES mosaico_board_platform mosaico_game mosaico_game_app mosaico_game_assets
             mosaico_game_2d mosaico_raylib_fast mosaico_raylib_port raylib
             esp_timer esp-mosaico-bsp)
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${RAYLIB_LITE_ENGINE_ROOT}/host/include")
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_GAME_NATIVE=1)
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_NATIVE_TARGET_FPS=50)
if(MOSAICO_NATIVE_IRIS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_NATIVE_IRIS=1)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${MOSAICO_UTILS_ROOT}/ESP-Iris/components/esp_iris/include"
        "${MOSAICO_RECOVERY_COMPONENT_DIR}/include")
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_iris idf::esp_mosaico_app_recovery)
endif()
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
set_source_files_properties(tomb_view.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
