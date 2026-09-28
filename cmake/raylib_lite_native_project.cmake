# Mosaico device examples use this dependency resolver. The only external
# input is the BSP; the engine does not discover repositories by sibling names.
# The ESP-Mosaico board port in ports/esp_mosaico is used unless the caller
# passes MOSAICO_BOARD_PLATFORM_DIR (and optionally the audio directories) for
# another board.
# Product firmware reuses a game by adding examples/<game>/main to
# EXTRA_COMPONENT_DIRS after including this file, and overrides the lifecycle
# hooks in raylib_lite_native_hooks.h from its own component.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

foreach(name MOSAICO_BSP_ROOT MOSAICO_BSP_COMPONENT_DIR MOSAICO_BOARD_PLATFORM_DIR
        MOSAICO_AUDIO_COMPONENT_DIR MOSAICO_AUDIO_PLATFORM_DIR)
    if(NOT ${name} AND DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
endforeach()
if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_BSP_ROOT)
    set(MOSAICO_BSP_COMPONENT_DIR "${MOSAICO_BSP_ROOT}/components/esp-mosaico-bsp")
endif()
if(NOT EXISTS "${MOSAICO_BSP_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Set MOSAICO_BSP_ROOT to an esp-mosaico-bsp checkout (or "
        "MOSAICO_BSP_COMPONENT_DIR to its components/esp-mosaico-bsp): "
        "'${MOSAICO_BSP_COMPONENT_DIR}'")
endif()
get_filename_component(MOSAICO_BSP_COMPONENT_DIR "${MOSAICO_BSP_COMPONENT_DIR}" ABSOLUTE)
set(MOSAICO_BSP_COMPONENT_DIR "${MOSAICO_BSP_COMPONENT_DIR}" CACHE PATH "esp-mosaico-bsp component")

include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_esp.cmake")
mosaico_game_sdk_add_components(RAYLIB TILEMAP FX SAVE)
list(APPEND EXTRA_COMPONENT_DIRS "${MOSAICO_BSP_COMPONENT_DIR}")

if(NOT MOSAICO_BOARD_PLATFORM_DIR)
    raylib_lite_esp_add_port()
    return()
endif()
foreach(name MOSAICO_BOARD_PLATFORM_DIR MOSAICO_AUDIO_COMPONENT_DIR MOSAICO_AUDIO_PLATFORM_DIR)
    if(${name} AND NOT EXISTS "${${name}}/CMakeLists.txt")
        message(FATAL_ERROR "Missing ${name}/CMakeLists.txt: '${${name}}'")
    endif()
endforeach()
if(MOSAICO_AUDIO_COMPONENT_DIR AND NOT MOSAICO_AUDIO_PLATFORM_DIR)
    message(FATAL_ERROR
        "MOSAICO_AUDIO_COMPONENT_DIR requires MOSAICO_AUDIO_PLATFORM_DIR=/path/to/platform_esp_audio")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${MOSAICO_BOARD_PLATFORM_DIR}"
    ${MOSAICO_AUDIO_COMPONENT_DIR} ${MOSAICO_AUDIO_PLATFORM_DIR})
