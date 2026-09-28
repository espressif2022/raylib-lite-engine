cmake_minimum_required(VERSION 3.16)

# The ESP-IDF configdep launcher cannot create dependency files for this
# example's deeply nested BSP/mbedTLS object paths. Keep plain idf.py build
# usable from the example directory.
set(CONFIGDEP_ENABLE OFF CACHE BOOL "Use the ESP-IDF configdep launcher" FORCE)
set(MOSAICO_NATIVE_IRIS OFF CACHE BOOL "Direct native application" FORCE)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdkconfig.native-direct.defaults")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_native_project.cmake")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(living_worlds VERSION 1.0.0)
