# Board-capable examples share this dependency resolver. No checkout layout is
# required: pass the two component directories, or a product/Vibe checkout.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_filename_component(_mosaico_checkout_parent "${RAYLIB_LITE_ENGINE_ROOT}/.." ABSOLUTE)

# A normal source checkout keeps these repositories beside the engine. This
# remains relative to the source tree, so moving the entire checkout works.
if(NOT MOSAICO_GAME_PATH AND
   EXISTS "${_mosaico_checkout_parent}/esp-mosaico-game/components/mosaico_board_platform/CMakeLists.txt")
    set(_mosaico_default_game_path "${_mosaico_checkout_parent}/esp-mosaico-game")
endif()
if(NOT MOSAICO_VIBE_PATH AND
   EXISTS "${_mosaico_checkout_parent}/esp-mosaico-vibe/submodule/esp-mosaico-bsp/components/esp-mosaico-bsp/CMakeLists.txt")
    set(_mosaico_default_vibe_path "${_mosaico_checkout_parent}/esp-mosaico-vibe")
endif()

foreach(name MOSAICO_GAME_PATH MOSAICO_VIBE_PATH)
    if(NOT ${name} AND DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
    if(NOT ${name})
        if(name STREQUAL "MOSAICO_GAME_PATH")
            set(${name} "${_mosaico_default_game_path}")
        else()
            set(${name} "${_mosaico_default_vibe_path}")
        endif()
    endif()
    if(${name})
        set(${name} "${${name}}" CACHE PATH "Mosaico dependency checkout")
    endif()
endforeach()

if(NOT MOSAICO_BOARD_PLATFORM_DIR AND MOSAICO_GAME_PATH)
    set(MOSAICO_BOARD_PLATFORM_DIR
        "${MOSAICO_GAME_PATH}/components/mosaico_board_platform")
endif()
if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_VIBE_PATH)
    set(MOSAICO_BSP_COMPONENT_DIR
        "${MOSAICO_VIBE_PATH}/submodule/esp-mosaico-bsp/components/esp-mosaico-bsp")
endif()
foreach(name MOSAICO_BOARD_PLATFORM_DIR MOSAICO_BSP_COMPONENT_DIR)
    if(NOT DEFINED ${name} AND DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
    if(NOT EXISTS "${${name}}/CMakeLists.txt")
        message(FATAL_ERROR
            "Set -D${name}=/path/to/component (or MOSAICO_GAME_PATH and "
            "MOSAICO_VIBE_PATH). Missing ${name}/CMakeLists.txt")
    endif()
    get_filename_component(${name} "${${name}}" ABSOLUTE)
    set(${name} "${${name}}" CACHE PATH "Mosaico component directory")
endforeach()

include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_esp.cmake")
mosaico_game_sdk_add_components(RAYLIB TILEMAP FX SAVE)
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_BOARD_PLATFORM_DIR}" "${MOSAICO_BSP_COMPONENT_DIR}")
if(NOT MOSAICO_AUDIO_COMPONENT_DIR AND DEFINED ENV{MOSAICO_AUDIO_COMPONENT_DIR})
    set(MOSAICO_AUDIO_COMPONENT_DIR "$ENV{MOSAICO_AUDIO_COMPONENT_DIR}")
endif()
if(NOT MOSAICO_AUDIO_PLATFORM_DIR AND DEFINED ENV{MOSAICO_AUDIO_PLATFORM_DIR})
    set(MOSAICO_AUDIO_PLATFORM_DIR "$ENV{MOSAICO_AUDIO_PLATFORM_DIR}")
endif()
if(NOT MOSAICO_AUDIO_COMPONENT_DIR AND MOSAICO_GAME_PATH)
    set(MOSAICO_AUDIO_COMPONENT_DIR "${MOSAICO_GAME_PATH}/components/mosaico_game_audio")
    set(MOSAICO_AUDIO_PLATFORM_DIR "${MOSAICO_GAME_PATH}/components/platform_esp_audio")
endif()
if(EXISTS "${MOSAICO_AUDIO_COMPONENT_DIR}/CMakeLists.txt")
    if(NOT EXISTS "${MOSAICO_AUDIO_PLATFORM_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "MOSAICO_AUDIO_COMPONENT_DIR requires MOSAICO_AUDIO_PLATFORM_DIR=/path/to/platform_esp_audio")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS
        "${MOSAICO_AUDIO_COMPONENT_DIR}" "${MOSAICO_AUDIO_PLATFORM_DIR}")
    set(MOSAICO_AUDIO_COMPONENT_DIR "${MOSAICO_AUDIO_COMPONENT_DIR}" CACHE PATH "Mosaico audio component")
    set(MOSAICO_AUDIO_PLATFORM_DIR "${MOSAICO_AUDIO_PLATFORM_DIR}" CACHE PATH "Mosaico audio platform component")
endif()

if(MOSAICO_NATIVE_IRIS)
    if(NOT MOSAICO_VIBE_PATH AND DEFINED ENV{MOSAICO_VIBE_PATH})
        set(MOSAICO_VIBE_PATH "$ENV{MOSAICO_VIBE_PATH}")
    endif()
    if(NOT EXISTS "${MOSAICO_VIBE_PATH}/cmake/mosaico_idf_project.cmake")
        message(FATAL_ERROR "Iris native requires -DMOSAICO_VIBE_PATH=/path/to/esp-mosaico-vibe")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS
        "${MOSAICO_VIBE_PATH}/submodule/esp-mosaico-utils/ESP-Iris/components/esp_iris"
        "${MOSAICO_VIBE_PATH}/components/esp_mosaico_app_recovery"
        "${RAYLIB_LITE_ENGINE_ROOT}/components/mosaico_iris_ota_size_check")
endif()
