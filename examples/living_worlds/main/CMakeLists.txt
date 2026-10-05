idf_component_register(
    SRCS "main.c" "living_worlds_native.c" "living_worlds_session.c"
         "living_worlds_scene_audio.c"
         "living_worlds_world.c"
         "living_worlds_aurora.c" "living_worlds_ocean.c"
         "living_worlds_volume.c" "living_worlds_view.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_ENGINE_ROOT}/examples/common"
    REQUIRES raylib-lite-engine esp_driver_jpeg)

target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_GAME_NATIVE=1)

set(asset_source "${CMAKE_CURRENT_LIST_DIR}/../assets_src")
set(asset_output "${CMAKE_CURRENT_BINARY_DIR}/living_worlds_assets")
set(volume_atlases
    sunrise_cliff_front sunrise_cliff_side sunrise_cliff_rear
    aurora_ice_front aurora_ice_side aurora_ice_rear
    ocean_reef_left_front ocean_reef_left_side ocean_reef_left_rear
    ocean_reef_right_front ocean_reef_right_side ocean_reef_right_rear
    rainforest_falls)
set(scene_sounds
    aurora_wind_ice ocean_ambience sunrise_wind rainforest_ambience)
file(GLOB asset_inputs CONFIGURE_DEPENDS "${asset_source}/*.json" "${asset_source}/*.png"
     "${asset_source}/*.jpg" "${asset_source}/*.wav")
set(asset_outputs)
foreach(name IN LISTS volume_atlases)
    list(APPEND asset_outputs "${asset_output}/${name}.atlas")
endforeach()
foreach(name IN LISTS scene_sounds)
    list(APPEND asset_outputs "${asset_output}/${name}.sound")
endforeach()
add_custom_command(
    OUTPUT ${asset_outputs}
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../tools/pack_game_assets.py"
            --source "${asset_source}" --output "${asset_output}" --limit 10485760
    DEPENDS ${asset_inputs} "${CMAKE_CURRENT_LIST_DIR}/../../../tools/pack_game_assets.py"
    VERBATIM)
add_custom_target(living_worlds_assets DEPENDS ${asset_outputs})
add_dependencies(${COMPONENT_LIB} living_worlds_assets)
foreach(name IN LISTS volume_atlases)
    target_add_binary_data(${COMPONENT_LIB} "${asset_output}/${name}.atlas" BINARY
        RENAME_TO ${name}_atlas DEPENDS living_worlds_assets)
endforeach()
foreach(name IN LISTS scene_sounds)
    target_add_binary_data(${COMPONENT_LIB} "${asset_output}/${name}.sound" BINARY
        RENAME_TO ${name}_sound DEPENDS living_worlds_assets)
endforeach()
foreach(name aurora ocean sunrise rainforest)
    target_add_binary_data(${COMPONENT_LIB} "${asset_source}/${name}.jpg" BINARY
        RENAME_TO ${name}_jpg)
endforeach()
set_source_files_properties(
    living_worlds_aurora.c living_worlds_ocean.c living_worlds_view.c
    living_worlds_volume.c
    PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")
