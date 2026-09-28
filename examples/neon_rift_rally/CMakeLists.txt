cmake_minimum_required(VERSION 3.16)

# Direct-native example project. Product launchers can reuse the same main/
# sources through their own board integration when MOSAICO_NATIVE_IRIS is on.
set(CONFIGDEP_ENABLE OFF CACHE BOOL "Use IDF configdep" FORCE)
set(MOSAICO_NATIVE_IRIS OFF CACHE BOOL "Direct native application" FORCE)
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdkconfig.native-direct.defaults"
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/raylib_lite_native_project.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(neon_rift_rally VERSION 1.0.0)
