# Shared CMake entry for examples/ in this repository.
# Include after setting SDKCONFIG_DEFAULTS. Call project() afterwards.
#
# Examples are standalone ESP-IDF projects. They do not require ESP-Iris or
# Recovery. Board support is pulled by the example idf_component.yml through
# the Component Manager.

get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/mosaico_game_sdk.cmake")

if(NOT MOSAICO_GAME_GSPC_FETCHER AND DEFINED ENV{MOSAICO_GAME_GSPC_FETCHER}
        AND NOT "$ENV{MOSAICO_GAME_GSPC_FETCHER}" STREQUAL "")
    set(MOSAICO_GAME_GSPC_FETCHER "$ENV{MOSAICO_GAME_GSPC_FETCHER}")
endif()

# project() must be a literal call in the example CMakeLists.txt.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
