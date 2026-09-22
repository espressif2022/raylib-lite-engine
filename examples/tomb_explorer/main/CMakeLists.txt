idf_component_register(
    SRCS "main.c" "tomb_app.c" "tomb_game.c" "tomb_view.c" "tomb_level_data.c"
    INCLUDE_DIRS "."
    REQUIRES mosaico_game_app mosaico_game_assets mosaico_game_2d
             mosaico_raylib_fast mosaico_raylib_port raylib esp-gsp)

set(GAME_ASSET_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../assets_src")
set(GAME_ASSET_OUTPUT "${CMAKE_CURRENT_LIST_DIR}/../assets/generated")
add_custom_target(tomb_explorer_assets ALL
    COMMAND "${PYTHON}" "${GAME_ASSET_SOURCE}/prepare_sprites.py"
    COMMAND "${PYTHON}" "${GAME_ASSET_SOURCE}/generate_textures.py"
    COMMAND "${PYTHON}" "${GAME_ASSET_SOURCE}/generate_level.py"
    COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../../tools/pack_game_assets.py"
            --source "${GAME_ASSET_SOURCE}" --output "${GAME_ASSET_OUTPUT}"
    BYPRODUCTS "${GAME_ASSET_OUTPUT}/textures.wall"
               "${GAME_ASSET_OUTPUT}/controls.atlas"
               "${CMAKE_CURRENT_LIST_DIR}/tomb_level_data.c"
    DEPENDS "${GAME_ASSET_SOURCE}/game_assets.json"
            "${GAME_ASSET_SOURCE}/textures.json"
            "${GAME_ASSET_SOURCE}/controls.json"
            "${GAME_ASSET_SOURCE}/level.json"
            "${GAME_ASSET_SOURCE}/prepare_sprites.py"
            "${GAME_ASSET_SOURCE}/generate_textures.py"
            "${GAME_ASSET_SOURCE}/generate_level.py")
add_dependencies(${COMPONENT_LIB} tomb_explorer_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/textures.wall" BINARY
    RENAME_TO textures_wall DEPENDS tomb_explorer_assets)
target_add_binary_data(${COMPONENT_LIB} "${GAME_ASSET_OUTPUT}/controls.atlas" BINARY
    RENAME_TO controls_atlas DEPENDS tomb_explorer_assets)
target_include_directories(${COMPONENT_LIB} PRIVATE "${GAME_ASSET_OUTPUT}")
set_source_files_properties(tomb_view.c PROPERTIES COMPILE_OPTIONS "-O3;-funroll-loops")

set(canvas_png "${CMAKE_CURRENT_BINARY_DIR}/canvas_480.png")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/make_canvas_png.py"
    "${canvas_png}" RESULT_VARIABLE canvas_result)
if(NOT canvas_result EQUAL 0)
    message(FATAL_ERROR "Could not generate Tomb Explorer Canvas placeholder")
endif()
set(CANVAS_IMAGE "${canvas_png}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/scene.json.in"
               "${CMAKE_CURRENT_BINARY_DIR}/scene.json" @ONLY)
gsp_add_bundle(${COMPONENT_LIB}
    SCENES "${CMAKE_CURRENT_BINARY_DIR}/scene.json"
    PIXEL_FORMAT rgb565)
