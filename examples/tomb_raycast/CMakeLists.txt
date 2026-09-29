cmake_minimum_required(VERSION 3.16)
set(CONFIGDEP_ENABLE OFF CACHE BOOL "Use IDF configdep" FORCE)
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdkconfig.native-direct.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_native_project.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tomb_raycast VERSION 1.0.0)
