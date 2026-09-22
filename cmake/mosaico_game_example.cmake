# Shared CMake entry for examples/ in this repository.
# Include after setting SDKCONFIG_DEFAULTS. Call project() afterwards.
#
# Examples are standalone ESP-IDF projects. They do not require ESP-Iris,
# Recovery, or ESP-Mosaico Vibe. Optional board support can be injected with
# MOSAICO_BSP_COMPONENT_DIR or MOSAICO_VIBE_ROOT.

get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/mosaico_game_sdk.cmake")

if(NOT MOSAICO_VIBE_ROOT AND DEFINED ENV{MOSAICO_VIBE_ROOT}
        AND NOT "$ENV{MOSAICO_VIBE_ROOT}" STREQUAL "")
    set(MOSAICO_VIBE_ROOT "$ENV{MOSAICO_VIBE_ROOT}")
endif()
if(NOT MOSAICO_BSP_COMPONENT_DIR AND DEFINED ENV{MOSAICO_BSP_COMPONENT_DIR}
        AND NOT "$ENV{MOSAICO_BSP_COMPONENT_DIR}" STREQUAL "")
    set(MOSAICO_BSP_COMPONENT_DIR "$ENV{MOSAICO_BSP_COMPONENT_DIR}")
endif()
if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_VIBE_ROOT)
    set(MOSAICO_BSP_COMPONENT_DIR
        "${MOSAICO_VIBE_ROOT}/submodule/esp-mosaico-bsp/components/esp-mosaico-bsp")
endif()
if(NOT MOSAICO_GAME_GSPC_FETCHER AND MOSAICO_VIBE_ROOT)
    set(MOSAICO_GAME_GSPC_FETCHER
        "${MOSAICO_VIBE_ROOT}/tools/gsp-sim/fetch_gspc.py")
endif()

function(mosaico_game_example_project NAME)
    cmake_parse_arguments(EXAMPLE "" "VERSION" "" ${ARGN})
    if(NOT EXAMPLE_VERSION)
        set(EXAMPLE_VERSION "0.1.0")
    endif()
    include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    project(${NAME} VERSION ${EXAMPLE_VERSION})
endfunction()
