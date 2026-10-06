# Native examples compose the board-neutral engine with one example-side board
# adapter selected by the application. The engine helper never knows a concrete
# BSP or board implementation.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT RAYLIB_LITE_BOARD AND DEFINED ENV{RAYLIB_LITE_BOARD})
    set(RAYLIB_LITE_BOARD "$ENV{RAYLIB_LITE_BOARD}")
endif()
if(NOT RAYLIB_LITE_BOARD)
    message(FATAL_ERROR
        "Set -DRAYLIB_LITE_BOARD=<board>; available adapters live under "
        "examples/boards/<board>")
endif()
set(RAYLIB_LITE_BOARD "${RAYLIB_LITE_BOARD}" CACHE STRING
    "Raylib Lite example board adapter")

set(_raylib_lite_board_dir
    "${RAYLIB_LITE_ENGINE_ROOT}/examples/boards/${RAYLIB_LITE_BOARD}")
if(NOT EXISTS "${_raylib_lite_board_dir}/board.cmake")
    message(FATAL_ERROR
        "Unknown RAYLIB_LITE_BOARD='${RAYLIB_LITE_BOARD}': missing "
        "${_raylib_lite_board_dir}/board.cmake")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${RAYLIB_LITE_ENGINE_ROOT}")
list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
include("${_raylib_lite_board_dir}/board.cmake")

if(NOT RAYLIB_LITE_BOARD_COMPONENT)
    message(FATAL_ERROR
        "Board '${RAYLIB_LITE_BOARD}' did not define RAYLIB_LITE_BOARD_COMPONENT")
endif()
if(RAYLIB_LITE_BOARD_SDKCONFIG_DEFAULTS)
    list(PREPEND SDKCONFIG_DEFAULTS "${RAYLIB_LITE_BOARD_SDKCONFIG_DEFAULTS}")
    list(REMOVE_DUPLICATES SDKCONFIG_DEFAULTS)
endif()

# Product firmware may reuse a game by adding examples/<game>/main to
# EXTRA_COMPONENT_DIRS after including this file. Product lifecycle hooks remain
# external to the engine and board adapter.
