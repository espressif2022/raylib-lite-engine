# Shared CMake entry for examples/ in this repository.
# Include after setting SDKCONFIG_DEFAULTS. Call project() afterwards.
#
# Host simulation only needs this engine. Device firmware still uses the
# ESP-Mosaico Vibe Recovery contract when MOSAICO_VIBE_ROOT is set or this
# repository is the vibe `submodule/raylib-lite-engine` checkout.

get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/mosaico_game_sdk.cmake")

if(NOT MOSAICO_VIBE_ROOT AND DEFINED ENV{MOSAICO_VIBE_ROOT}
        AND NOT "$ENV{MOSAICO_VIBE_ROOT}" STREQUAL "")
    set(MOSAICO_VIBE_ROOT "$ENV{MOSAICO_VIBE_ROOT}")
endif()
if(NOT MOSAICO_VIBE_ROOT)
    get_filename_component(_mosaico_maybe_vibe
        "${RAYLIB_LITE_ENGINE_ROOT}/../.." ABSOLUTE)
    if(EXISTS "${_mosaico_maybe_vibe}/cmake/mosaico_idf_project.cmake")
        set(MOSAICO_VIBE_ROOT "${_mosaico_maybe_vibe}")
    endif()
    unset(_mosaico_maybe_vibe)
endif()

if(MOSAICO_VIBE_ROOT)
    if(NOT MOSAICO_GAME_GSPC_FETCHER)
        set(MOSAICO_GAME_GSPC_FETCHER
            "${MOSAICO_VIBE_ROOT}/tools/gsp-sim/fetch_gspc.py")
    endif()
    if(NOT MOSAICO_GAME_RECOVERY_COMPONENT_DIR)
        set(MOSAICO_GAME_RECOVERY_COMPONENT_DIR
            "${MOSAICO_VIBE_ROOT}/components/esp_mosaico_app_recovery")
    endif()
endif()

function(mosaico_game_example_project NAME)
    cmake_parse_arguments(EXAMPLE "" "VERSION" "" ${ARGN})
    if(NOT EXAMPLE_VERSION)
        set(EXAMPLE_VERSION "0.1.0")
    endif()
    if(MOSAICO_VIBE_ROOT AND EXISTS "${MOSAICO_VIBE_ROOT}/cmake/mosaico_idf_project.cmake")
        include("${MOSAICO_VIBE_ROOT}/cmake/mosaico_idf_project.cmake")
    else()
        include($ENV{IDF_PATH}/tools/cmake/project.cmake)
    endif()
    project(${NAME} VERSION ${EXAMPLE_VERSION})
    if(MOSAICO_VIBE_ROOT AND EXISTS "${MOSAICO_VIBE_ROOT}/cmake/system_update.cmake")
        include("${MOSAICO_VIBE_ROOT}/cmake/system_update.cmake")
    endif()
endfunction()
