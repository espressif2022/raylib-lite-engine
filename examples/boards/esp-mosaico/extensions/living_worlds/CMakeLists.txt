idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
    raylib-lite-engine COMPONENT_DIR)
idf_component_get_property(_living_worlds_main main COMPONENT_DIR)
if(NOT _living_worlds_main)
    message(FATAL_ERROR "Living Worlds Board extension could not resolve the main component")
endif()
get_filename_component(_living_worlds_root "${_living_worlds_main}/.." ABSOLUTE)
set(_asset_source "${_living_worlds_root}/assets_src")
set(_asset_output "${CMAKE_CURRENT_BINARY_DIR}/living_worlds_assets")

idf_component_register(
    SRCS "living_worlds_native_assets.c"
    PRIV_INCLUDE_DIRS
        "${_living_worlds_main}"
    REQUIRES esp-mosaico esp_driver_jpeg log)

set(_volume_atlases
    sunrise_cliff_front sunrise_cliff_side sunrise_cliff_rear
    aurora_ice_front aurora_ice_side aurora_ice_rear
    ocean_reef_left_front ocean_reef_left_side ocean_reef_left_rear
    ocean_reef_right_front ocean_reef_right_side ocean_reef_right_rear
    rainforest_falls)
set(_scene_sounds
    aurora_wind_ice ocean_ambience sunrise_wind rainforest_ambience)
file(GLOB _asset_inputs CONFIGURE_DEPENDS
    "${_asset_source}/*.json" "${_asset_source}/*.png"
    "${_asset_source}/*.jpg" "${_asset_source}/*.wav")
set(_asset_outputs)
foreach(name IN LISTS _volume_atlases)
    list(APPEND _asset_outputs "${_asset_output}/${name}.atlas")
endforeach()
foreach(name IN LISTS _scene_sounds)
    list(APPEND _asset_outputs "${_asset_output}/${name}.sound")
endforeach()
add_custom_command(
    OUTPUT ${_asset_outputs}
    COMMAND "${PYTHON}" "${RAYLIB_LITE_ENGINE_ROOT}/tools/pack_game_assets.py"
            --source "${_asset_source}" --output "${_asset_output}" --limit 10485760
    DEPENDS ${_asset_inputs} "${RAYLIB_LITE_ENGINE_ROOT}/tools/pack_game_assets.py"
    VERBATIM)
add_custom_target(living_worlds_native_assets_gen DEPENDS ${_asset_outputs})
add_dependencies(${COMPONENT_LIB} living_worlds_native_assets_gen)
foreach(name IN LISTS _volume_atlases)
    target_add_binary_data(${COMPONENT_LIB} "${_asset_output}/${name}.atlas" BINARY
        RENAME_TO ${name}_atlas DEPENDS living_worlds_native_assets_gen)
endforeach()
foreach(name IN LISTS _scene_sounds)
    target_add_binary_data(${COMPONENT_LIB} "${_asset_output}/${name}.sound" BINARY
        RENAME_TO ${name}_sound DEPENDS living_worlds_native_assets_gen)
endforeach()
foreach(name aurora ocean sunrise rainforest)
    target_add_binary_data(${COMPONENT_LIB} "${_asset_source}/${name}.jpg" BINARY
        RENAME_TO ${name}_jpg)
endforeach()
