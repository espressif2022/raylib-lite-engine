cmake_minimum_required(VERSION 3.19)
project(vertical_dock C)

get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(MOSAICO_SDK_ROOT "${RAYLIB_LITE_ENGINE_ROOT}/../esp-mosaico-elf-game-sdk" ABSOLUTE)
include("${MOSAICO_SDK_ROOT}/cmake/mosaico-game.cmake")

set(VERTICAL_DOCK_SOURCE_ROOT "${RAYLIB_LITE_ENGINE_ROOT}/examples/vertical_dock")
add_mosaico_game(vertical_dock SOURCE_ROOT "${VERTICAL_DOCK_SOURCE_ROOT}"
    ASSET_ROOT "${VERTICAL_DOCK_SOURCE_ROOT}")
target_include_directories(vertical_dock_module PRIVATE
    "${VERTICAL_DOCK_SOURCE_ROOT}/main"
    "${RAYLIB_LITE_ENGINE_ROOT}/components/mosaico_game_2d/include")
target_compile_definitions(vertical_dock_module PRIVATE ESP_PLATFORM)
