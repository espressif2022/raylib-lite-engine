set(RAYLIB_LITE_EXAMPLE_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common")
idf_component_register(
    SRCS "game_module.c"
         "living_worlds_session.c" "living_worlds_scene_audio.c"
         "living_worlds_world.c" "living_worlds_aurora.c" "living_worlds_ocean.c"
         "living_worlds_volume.c" "living_worlds_view.c"
         "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}/native_module_main.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}"
    REQUIRES ${RAYLIB_LITE_BOARD} living_worlds)

target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)

set_source_files_properties(
    living_worlds_aurora.c living_worlds_ocean.c living_worlds_view.c
    living_worlds_volume.c
    PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
