file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_ENGINE_ROOT}/examples/common/native_module_main.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_board_platform mosaico_game_app mosaico_game_assets
             mosaico_game_2d mosaico_raylib_fast mosaico_raylib_port raylib mosaico_game_tilemap mosaico_game_audio)
target_include_directories(${COMPONENT_LIB} SYSTEM PRIVATE "${RAYLIB_LITE_ENGINE_ROOT}/host/include")
target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_GAME_NATIVE=1)
if(MOSAICO_NATIVE_IRIS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_NATIVE_IRIS=1)
    target_include_directories(${COMPONENT_LIB} PRIVATE
        "${MOSAICO_VIBE_PATH}/submodule/esp-mosaico-utils/ESP-Iris/components/esp_iris/include"
        "${MOSAICO_VIBE_PATH}/components/esp_mosaico_app_recovery/include")
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp_iris idf::esp_mosaico_app_recovery)
endif()
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
