# Project integration for Raylib Lite Engine.
set(MOSAICO_GAME_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")

set(MOSAICO_GAME_GSPC_FETCHER "" CACHE FILEPATH
    "Optional script that prints the path to a GSP compiler")

function(mosaico_game_sdk_configure_gsp_compiler)
    # Do not call add_compile_options() here: this runs before project() and
    # would make CMake pick the host compiler.
    if(DEFINED GSPC_EXECUTABLE OR DEFINED ENV{GSPC_EXECUTABLE})
        return()
    endif()
    find_program(_mosaico_python NAMES python3 python)
    set(_mosaico_fetch_gspc "${MOSAICO_GAME_GSPC_FETCHER}")
    if(_mosaico_python AND EXISTS "${_mosaico_fetch_gspc}")
        execute_process(
            COMMAND "${_mosaico_python}" "${_mosaico_fetch_gspc}"
            OUTPUT_VARIABLE _mosaico_cached_gspc
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _mosaico_gspc_result)
        if(_mosaico_gspc_result EQUAL 0 AND EXISTS "${_mosaico_cached_gspc}")
            set(GSPC_EXECUTABLE "${_mosaico_cached_gspc}" CACHE FILEPATH
                "Standalone ESP-GSP scene compiler")
            return()
        endif()
    endif()
    find_program(_mosaico_gspc NAMES gspc gspc-dev)
    if(_mosaico_gspc)
        set(GSPC_EXECUTABLE "${_mosaico_gspc}" CACHE FILEPATH
            "ESP-GSP scene compiler")
    endif()
endfunction()

function(mosaico_game_sdk_add_components)
    set(options RAYLIB AUDIO TILEMAP SCENE UI FX SAVE)
    cmake_parse_arguments(GAME "${options}" "" "" ${ARGN})

    set(_components mosaico_game mosaico_game_input mosaico_game_debug)
    if(GAME_RAYLIB OR GAME_AUDIO OR GAME_TILEMAP)
        list(APPEND _components mosaico_game_assets)
    endif()
    if(GAME_RAYLIB OR GAME_TILEMAP)
        list(APPEND _components mosaico_game_2d)
    endif()
    if(GAME_RAYLIB)
        list(APPEND _components mosaico_raylib_fast mosaico_raylib_port mosaico_game_app)
    endif()
    if(GAME_AUDIO)
        list(APPEND _components mosaico_game_audio)
    endif()
    if(GAME_TILEMAP)
        list(APPEND _components mosaico_game_tilemap)
    endif()
    if(GAME_SCENE)
        list(APPEND _components mosaico_game_scene)
    endif()
    if(GAME_UI)
        list(APPEND _components mosaico_game_ui)
    endif()
    if(GAME_FX)
        list(APPEND _components mosaico_game_fx)
    endif()
    if(GAME_SAVE)
        list(APPEND _components mosaico_game_save)
    endif()

    foreach(_component IN LISTS _components)
        list(APPEND EXTRA_COMPONENT_DIRS
            "${MOSAICO_GAME_SDK_ROOT}/components/${_component}")
    endforeach()
    list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" PARENT_SCOPE)
endfunction()
