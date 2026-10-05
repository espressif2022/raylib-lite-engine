# ESP-IDF integration for Raylib Lite Engine.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Options are retained during the migration so existing callers do not need a
# flag-day CMake change. The engine is now one IDF component; the linker still
# discards unreferenced objects from its static library.
function(mosaico_game_sdk_add_components)
    cmake_parse_arguments(GAME "RAYLIB;AUDIO;TILEMAP;SCENE;UI;FX;SAVE" "" "" ${ARGN})
    list(APPEND EXTRA_COMPONENT_DIRS
        "${RAYLIB_LITE_ENGINE_ROOT}/components/raylib_lite_engine")
    list(REMOVE_DUPLICATES EXTRA_COMPONENT_DIRS)
    set(EXTRA_COMPONENT_DIRS "${EXTRA_COMPONENT_DIRS}" PARENT_SCOPE)
endfunction()
