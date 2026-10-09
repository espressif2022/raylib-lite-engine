# Pre-project settings required by the ESP-Mosaico application Board.
set(ESP_IRIS_BUILD_PROFILE "usb" CACHE STRING
    "ESP-Mosaico native examples use USB-only ESP-Iris" FORCE)

# Optional vibe checkouts. The packaged manifest keeps its Git pins; a local
# component of the same name is selected ahead of that download.
foreach(_input RAYLIB_LITE_UTILS_DIR RAYLIB_LITE_BSP_DIR)
    if(NOT ${_input} AND DEFINED ENV{${_input}})
        set(${_input} "$ENV{${_input}}")
    endif()
endforeach()
if(RAYLIB_LITE_UTILS_DIR)
    get_filename_component(RAYLIB_LITE_UTILS_DIR "${RAYLIB_LITE_UTILS_DIR}" ABSOLUTE)
    set(_raylib_lite_utils_root "${RAYLIB_LITE_UTILS_DIR}")
    if(EXISTS "${RAYLIB_LITE_UTILS_DIR}/CMakeLists.txt" AND
            NOT EXISTS "${RAYLIB_LITE_UTILS_DIR}/ESP-Iris/components/esp_iris/CMakeLists.txt")
        get_filename_component(_utils_component_name "${RAYLIB_LITE_UTILS_DIR}" NAME)
        if(NOT _utils_component_name STREQUAL "esp_iris")
            message(FATAL_ERROR "Expected esp_iris component or complete utils checkout, got ${RAYLIB_LITE_UTILS_DIR}")
        endif()
        get_filename_component(_candidate "${RAYLIB_LITE_UTILS_DIR}/../../.." ABSOLUTE)
        if(EXISTS "${_candidate}/esp-mosaico-recovery/components/esp_mosaico_app_recovery/CMakeLists.txt")
            set(_raylib_lite_utils_root "${_candidate}")
        endif()
    endif()
    if(NOT EXISTS "${_raylib_lite_utils_root}/ESP-Iris/components/esp_iris/CMakeLists.txt" OR
            NOT EXISTS "${_raylib_lite_utils_root}/esp-mosaico-recovery/components/esp_mosaico_app_recovery/CMakeLists.txt")
        message(FATAL_ERROR
            "RAYLIB_LITE_UTILS_DIR must select a complete utils checkout or its esp_iris component; both Iris and Recovery are required")
    endif()
    if(EXISTS "${_raylib_lite_utils_root}/esp-mosaico-recovery/components/esp_mosaico_app_recovery/CMakeLists.txt")
        if(FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS AND
                NOT FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS STREQUAL _raylib_lite_utils_root)
            message(FATAL_ERROR "Local Iris and Recovery must use the same utils checkout; use a fresh build directory")
        endif()
        set(FETCHCONTENT_SOURCE_DIR_RAYLIB_LITE_MOSAICO_UTILS "${_raylib_lite_utils_root}")
    endif()
endif()

function(_raylib_lite_add_local_component root component)
    if(EXISTS "${root}/CMakeLists.txt" AND EXISTS "${root}/idf_component.yml")
        get_filename_component(_local_name "${root}" NAME)
        if(NOT _local_name STREQUAL component)
            message(FATAL_ERROR "Expected local component ${component}, got ${root}")
        endif()
        list(APPEND EXTRA_COMPONENT_DIRS "${root}")
        set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" PARENT_SCOPE)
        return()
    endif()
    if(EXISTS "${root}/${component}/CMakeLists.txt")
        list(APPEND EXTRA_COMPONENT_DIRS "${root}/${component}")
        set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" PARENT_SCOPE)
        return()
    endif()
    message(FATAL_ERROR
        "Local component ${component} was not found under ${root}")
endfunction()

if(RAYLIB_LITE_BSP_DIR)
    set(_raylib_lite_bsp "${RAYLIB_LITE_BSP_DIR}")
    set(_raylib_lite_splash "")
    if(EXISTS "${_raylib_lite_bsp}/components/esp-mosaico-bsp/CMakeLists.txt")
        set(_raylib_lite_splash "${_raylib_lite_bsp}/components/mosaico_boot_splash")
        set(_raylib_lite_bsp "${_raylib_lite_bsp}/components")
    elseif(EXISTS "${_raylib_lite_bsp}/CMakeLists.txt")
        get_filename_component(_raylib_lite_splash "${_raylib_lite_bsp}" DIRECTORY)
        set(_raylib_lite_splash "${_raylib_lite_splash}/mosaico_boot_splash")
    endif()
    _raylib_lite_add_local_component("${_raylib_lite_bsp}" "esp-mosaico-bsp")
    if(EXISTS "${_raylib_lite_splash}/CMakeLists.txt")
        list(APPEND EXTRA_COMPONENT_DIRS "${_raylib_lite_splash}")
    endif()
endif()

if(RAYLIB_LITE_UTILS_DIR)
    set(_raylib_lite_utils "${_raylib_lite_utils_root}")
    if(EXISTS "${_raylib_lite_utils}/ESP-Iris/components/esp_iris/CMakeLists.txt")
        list(APPEND EXTRA_COMPONENT_DIRS
            "${_raylib_lite_utils}/ESP-Iris/components/esp_iris")
    else()
        _raylib_lite_add_local_component("${_raylib_lite_utils}" "esp_iris")
    endif()
endif()

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
