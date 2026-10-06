# ESP-Mosaico reference-board selection for native examples.
set(RAYLIB_LITE_BOARD_COMPONENT "esp-mosaico" CACHE INTERNAL
    "Selected Raylib Lite example-board component" FORCE)
set(RAYLIB_LITE_BOARD_SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults" CACHE INTERNAL
    "Selected Raylib Lite board sdkconfig defaults" FORCE)

foreach(name MOSAICO_BSP_ROOT MOSAICO_BSP_COMPONENT_DIR)
    if(NOT ${name} AND DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
endforeach()

if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_BSP_ROOT)
    set(MOSAICO_BSP_COMPONENT_DIR
        "${MOSAICO_BSP_ROOT}/components/esp-mosaico-bsp")
endif()
if(NOT EXISTS "${MOSAICO_BSP_COMPONENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "ESP-Mosaico requires MOSAICO_BSP_ROOT=/path/to/esp-mosaico-bsp "
        "or MOSAICO_BSP_COMPONENT_DIR=/path/to/components/esp-mosaico-bsp")
endif()

get_filename_component(MOSAICO_BSP_COMPONENT_DIR
    "${MOSAICO_BSP_COMPONENT_DIR}" ABSOLUTE)
set(MOSAICO_BSP_COMPONENT_DIR "${MOSAICO_BSP_COMPONENT_DIR}" CACHE PATH
    "ESP-Mosaico BSP component")

list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}"
    "${MOSAICO_BSP_COMPONENT_DIR}")

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
