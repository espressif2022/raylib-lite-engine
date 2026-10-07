# Application-side Board composition for repository examples.
# Engine code must not include or depend on this helper.
set(RAYLIB_LITE_BOARD "esp-mosaico" CACHE STRING
    "Raylib Lite example Board component")

get_filename_component(_raylib_lite_boards_root
    "${CMAKE_CURRENT_LIST_DIR}/../boards" ABSOLUTE)
file(GLOB _raylib_lite_board_candidates RELATIVE "${_raylib_lite_boards_root}"
    "${_raylib_lite_boards_root}/*")
set(_raylib_lite_available_boards)
foreach(_candidate IN LISTS _raylib_lite_board_candidates)
    if(EXISTS "${_raylib_lite_boards_root}/${_candidate}/CMakeLists.txt" AND
            EXISTS "${_raylib_lite_boards_root}/${_candidate}/idf_component.yml")
        list(APPEND _raylib_lite_available_boards "${_candidate}")
    endif()
endforeach()
list(SORT _raylib_lite_available_boards)
set_property(CACHE RAYLIB_LITE_BOARD PROPERTY STRINGS ${_raylib_lite_available_boards})

set(RAYLIB_LITE_BOARD_DIR
    "${_raylib_lite_boards_root}/${RAYLIB_LITE_BOARD}")
if(NOT EXISTS "${RAYLIB_LITE_BOARD_DIR}/CMakeLists.txt" OR
        NOT EXISTS "${RAYLIB_LITE_BOARD_DIR}/idf_component.yml")
    string(JOIN ", " _available ${_raylib_lite_available_boards})
    message(FATAL_ERROR
        "Unknown RAYLIB_LITE_BOARD='${RAYLIB_LITE_BOARD}'. Available: ${_available}")
endif()

list(APPEND EXTRA_COMPONENT_DIRS "${RAYLIB_LITE_BOARD_DIR}")

set(_raylib_lite_board_defaults "${RAYLIB_LITE_BOARD_DIR}/sdkconfig.defaults")
if(EXISTS "${_raylib_lite_board_defaults}")
    list(PREPEND SDKCONFIG_DEFAULTS "${_raylib_lite_board_defaults}")
endif()

if(NOT SDKCONFIG)
    set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig" CACHE FILEPATH "Per-build config")
endif()

set(CONFIGDEP_ENABLE OFF CACHE BOOL "Use IDF configdep" FORCE)

set(_raylib_lite_board_project "${RAYLIB_LITE_BOARD_DIR}/project.cmake")
if(EXISTS "${_raylib_lite_board_project}")
    include("${_raylib_lite_board_project}")
endif()
