# ESP-Mosaico reference-board selection for native examples.
set(RAYLIB_LITE_BOARD_COMPONENT "esp-mosaico" CACHE INTERNAL
    "Selected Raylib Lite example-board component" FORCE)
set(RAYLIB_LITE_BOARD_SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults" CACHE INTERNAL
    "Selected Raylib Lite board sdkconfig defaults" FORCE)

# Keep generated sdkconfig state in the selected build directory. Native example
# source trees may contain stale local sdkconfig files from earlier builds; board
# defaults must remain authoritative for Iris transport and OTA layout.
if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH
        "ESP-Mosaico native-example sdkconfig" FORCE)
endif()

foreach(name MOSAICO_BSP_ROOT MOSAICO_BSP_COMPONENT_DIR
             MOSAICO_UTILS_ROOT ESP_IRIS_COMPONENT_DIR)
    if((NOT DEFINED ${name} OR "${${name}}" STREQUAL "") AND
            DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
endforeach()

if((NOT DEFINED MOSAICO_UTILS_ROOT OR "${MOSAICO_UTILS_ROOT}" STREQUAL "") AND
        DEFINED ENV{MOSAICO_DEPS_ROOT})
    set(MOSAICO_UTILS_ROOT "$ENV{MOSAICO_DEPS_ROOT}/esp-mosaico-utils")
elseif((NOT DEFINED MOSAICO_UTILS_ROOT OR "${MOSAICO_UTILS_ROOT}" STREQUAL "") AND
        DEFINED ENV{MOSAICO_VIBE_PATH})
    set(MOSAICO_UTILS_ROOT "$ENV{MOSAICO_VIBE_PATH}/submodule/esp-mosaico-utils")
endif()
if((NOT DEFINED ESP_IRIS_COMPONENT_DIR OR "${ESP_IRIS_COMPONENT_DIR}" STREQUAL "") AND
        DEFINED MOSAICO_UTILS_ROOT AND NOT "${MOSAICO_UTILS_ROOT}" STREQUAL "")
    set(ESP_IRIS_COMPONENT_DIR
        "${MOSAICO_UTILS_ROOT}/ESP-Iris/components/esp_iris")
endif()

if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_BSP_ROOT)
    set(MOSAICO_BSP_COMPONENT_DIR
        "${MOSAICO_BSP_ROOT}/components/esp-mosaico-bsp")
endif()
if(NOT EXISTS "${MOSAICO_BSP_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ESP-Mosaico requires MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp "
        "or MOSAICO_BSP_COMPONENT_DIR=/path/to/components/esp-mosaico-bsp")
endif()

if(NOT EXISTS "${ESP_IRIS_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ESP-Mosaico native examples require ESP-Iris. Set "
        "MOSAICO_UTILS_ROOT=/path/to/esp-mosaico-utils or "
        "ESP_IRIS_COMPONENT_DIR=/path/to/ESP-Iris/components/esp_iris")
endif()

get_filename_component(MOSAICO_BSP_COMPONENT_DIR
    "${MOSAICO_BSP_COMPONENT_DIR}" ABSOLUTE)
get_filename_component(ESP_IRIS_COMPONENT_DIR
    "${ESP_IRIS_COMPONENT_DIR}" ABSOLUTE)
set(MOSAICO_BSP_COMPONENT_DIR "${MOSAICO_BSP_COMPONENT_DIR}" CACHE PATH
    "ESP-Mosaico BSP component")
set(ESP_IRIS_COMPONENT_DIR "${ESP_IRIS_COMPONENT_DIR}" CACHE PATH
    "ESP-Iris component")
set(ESP_IRIS_BUILD_PROFILE "usb" CACHE STRING
    "ESP-Mosaico native examples use USB-only ESP-Iris" FORCE)

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}"
    "${MOSAICO_BSP_COMPONENT_DIR}"
    "${ESP_IRIS_COMPONENT_DIR}")

if(RAYLIB_LITE_BOARD_GAME_EXTENSION)
    set(_raylib_lite_board_extension_dir
        "${CMAKE_CURRENT_LIST_DIR}/extensions/${RAYLIB_LITE_BOARD_GAME_EXTENSION}")
    if(NOT EXISTS "${_raylib_lite_board_extension_dir}/CMakeLists.txt")
        message(FATAL_ERROR
            "ESP-Mosaico has no Board extension for "
            "RAYLIB_LITE_BOARD_GAME_EXTENSION='${RAYLIB_LITE_BOARD_GAME_EXTENSION}'")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS "${_raylib_lite_board_extension_dir}")
    set(RAYLIB_LITE_BOARD_GAME_COMPONENTS
        "${RAYLIB_LITE_BOARD_GAME_EXTENSION}" CACHE INTERNAL
        "Selected Board-specific game extension components" FORCE)
else()
    set(RAYLIB_LITE_BOARD_GAME_COMPONENTS "" CACHE INTERNAL
        "Selected Board-specific game extension components" FORCE)
endif()
list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
