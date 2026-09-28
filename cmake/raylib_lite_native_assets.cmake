# Called from an example's main component after idf_component_register().
function(raylib_lite_native_embed_assets example_root)
    set(original_source_dir "${example_root}/assets_src")
    set(stage_root "${CMAKE_CURRENT_BINARY_DIR}/native_example")
    set(source_dir "${stage_root}/assets_src")
    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/native_assets")
    file(GLOB original_sources "${original_source_dir}/*")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        ${original_sources} "${RAYLIB_LITE_ENGINE_ROOT}/tools/pack_game_assets.py")
    # Hooks write generated sprites/audio next to __file__. Run them against a
    # private copy so parallel direct/Iris builds never race on source assets.
    file(REMOVE_RECURSE "${stage_root}")
    file(MAKE_DIRECTORY "${source_dir}")
    file(COPY "${original_source_dir}/" DESTINATION "${source_dir}")
    file(GLOB prepare_hooks "${source_dir}/prepare_*.py")
    file(GLOB generate_hooks "${source_dir}/generate_*.py")
    foreach(hook IN LISTS prepare_hooks generate_hooks)
        get_filename_component(hook_name "${hook}" NAME)
        if(hook_name STREQUAL "generate_level.py")
            # Tomb's level generator writes gameplay C, not an asset. Keep the
            # one checked-in engine source authoritative and verify parity.
            execute_process(COMMAND "${PYTHON}"
                "${original_source_dir}/${hook_name}" --check
                WORKING_DIRECTORY "${example_root}"
                RESULT_VARIABLE hook_result ERROR_VARIABLE hook_error)
        else()
            execute_process(COMMAND "${PYTHON}" "${hook}"
                WORKING_DIRECTORY "${stage_root}"
                RESULT_VARIABLE hook_result ERROR_VARIABLE hook_error)
        endif()
        if(NOT hook_result EQUAL 0)
            message(FATAL_ERROR "Asset preparation failed for ${hook}: ${hook_error}")
        endif()
    endforeach()
    file(MAKE_DIRECTORY "${output_dir}")
    # The asset packer determines outputs from game_assets.json. Running it at
    # configure time lets CMake discover those names for target_add_binary_data.
    execute_process(
        COMMAND "${PYTHON}" "${RAYLIB_LITE_ENGINE_ROOT}/tools/pack_game_assets.py"
            --source "${source_dir}" --output "${output_dir}" --limit 10485760
        RESULT_VARIABLE pack_result OUTPUT_QUIET ERROR_VARIABLE pack_error)
    if(NOT pack_result EQUAL 0)
        message(FATAL_ERROR "Asset pack failed for ${example_root}: ${pack_error}")
    endif()
    file(GLOB packed_assets "${output_dir}/*.atlas" "${output_dir}/*.wall"
        "${output_dir}/*.map" "${output_dir}/*.sound" "${output_dir}/*.jpg")
    target_include_directories(${COMPONENT_LIB} PRIVATE "${output_dir}")
    set(register_c "${CMAKE_CURRENT_BINARY_DIR}/register_native_assets.c")
    file(WRITE "${register_c}" "#include \"mosaico_game_assets.h\"\nvoid raylib_lite_register_native_assets(void) {\n")
    foreach(asset IN LISTS packed_assets)
        get_filename_component(name "${asset}" NAME)
        string(REPLACE "." "_" symbol "${name}")
        string(REPLACE "-" "_" symbol "${symbol}")
        target_add_binary_data(${COMPONENT_LIB} "${asset}" BINARY RENAME_TO "${symbol}")
        file(APPEND "${register_c}"
            "  extern const unsigned char _binary_${symbol}_start[];\n"
            "  extern const unsigned char _binary_${symbol}_end[];\n"
            "  mosaico_game_asset_register_memory(\"${name}\", _binary_${symbol}_start, _binary_${symbol}_end - _binary_${symbol}_start);\n")
    endforeach()
    file(APPEND "${register_c}" "}\n")
    target_sources(${COMPONENT_LIB} PRIVATE "${register_c}")
endfunction()
