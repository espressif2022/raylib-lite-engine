cmake_minimum_required(VERSION 3.19)
project(sewer_labyrinth C)

get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(MOSAICO_SDK_ROOT "${RAYLIB_LITE_ENGINE_ROOT}/../esp-mosaico-elf-game-sdk" ABSOLUTE)
include("${MOSAICO_SDK_ROOT}/cmake/mosaico-game.cmake")

set(SEWER_SOURCE_ROOT "${RAYLIB_LITE_ENGINE_ROOT}/examples/sewer_labyrinth")
add_mosaico_game(sewer_labyrinth SOURCE_ROOT "${SEWER_SOURCE_ROOT}"
    ASSET_ROOT "${SEWER_SOURCE_ROOT}")
target_include_directories(sewer_labyrinth_module PRIVATE
    "${SEWER_SOURCE_ROOT}/main"
    "${RAYLIB_LITE_ENGINE_ROOT}/include/raylib_lite")
target_compile_definitions(sewer_labyrinth_module PRIVATE ESP_PLATFORM)
