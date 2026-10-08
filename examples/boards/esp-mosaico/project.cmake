# Pre-project settings required by the ESP-Mosaico application Board.
set(ESP_IRIS_BUILD_PROFILE "usb" CACHE STRING
    "ESP-Mosaico native examples use USB-only ESP-Iris" FORCE)

# Recovery's private headers live outside its component directory. Fetch the
# complete pinned upstream tree until Recovery supports standalone packaging.
# Keep this application dependency outside the Engine component.
include(FetchContent)
FetchContent_Declare(raylib_lite_mosaico_utils
    GIT_REPOSITORY https://github.com/espressif2022/esp-mosaico-utils.git
    GIT_TAG e9509a4cd6323d64bd0d2a7c432c96654de7a633
    GIT_SUBMODULES ""
    GIT_PROGRESS TRUE)
FetchContent_GetProperties(raylib_lite_mosaico_utils)
if(NOT raylib_lite_mosaico_utils_POPULATED)
    FetchContent_Populate(raylib_lite_mosaico_utils)
endif()
set(_raylib_lite_recovery_dir
    "${raylib_lite_mosaico_utils_SOURCE_DIR}/esp-mosaico-recovery/components/esp_mosaico_app_recovery")
if(NOT EXISTS "${_raylib_lite_recovery_dir}/CMakeLists.txt")
    message(FATAL_ERROR "Pinned utilities checkout has no Recovery application component")
endif()
list(APPEND EXTRA_COMPONENT_DIRS "${_raylib_lite_recovery_dir}")
message(STATUS "Raylib Lite Recovery component: ${_raylib_lite_recovery_dir}")
