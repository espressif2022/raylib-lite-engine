idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
    raylib-lite-engine COMPONENT_DIR)
idf_component_register(
    SRCS "game_module.c"
         "living_worlds_session.c" "living_worlds_scene_audio.c"
         "living_worlds_world.c" "living_worlds_aurora.c" "living_worlds_ocean.c"
         "living_worlds_volume.c" "living_worlds_view.c"
         "native/living_worlds_native_assets.c"
    INCLUDE_DIRS "."
    REQUIRES examples_common ${RAYLIB_LITE_BOARD} esp_driver_jpeg log)

target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/.."
    EXCLUDE aurora.atlas ocean.atlas sunrise.atlas rainforest.atlas
    EXTRA_FILES aurora.jpg ocean.jpg sunrise.jpg rainforest.jpg)

set_source_files_properties(
    living_worlds_aurora.c living_worlds_ocean.c living_worlds_view.c
    living_worlds_volume.c
    PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
