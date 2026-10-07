idf_component_get_property(RAYLIB_LITE_ENGINE_ROOT
    raylib-lite-engine COMPONENT_DIR)
file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources}
    INCLUDE_DIRS "."
    REQUIRES examples_common ${RAYLIB_LITE_BOARD}
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
if(DEFINED VERTICAL_DOCK_SUBDIVIDE_SURFACES)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        VD_SUBDIVIDE_SURFACES=${VERTICAL_DOCK_SUBDIVIDE_SURFACES})
endif()
if(DEFINED VERTICAL_DOCK_FACE_SEGMENT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        VD_FACE_SEGMENT=${VERTICAL_DOCK_FACE_SEGMENT})
endif()
if(DEFINED VERTICAL_DOCK_SCENE_DETAIL)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        VD_SCENE_DETAIL=${VERTICAL_DOCK_SCENE_DETAIL})
endif()
include("${RAYLIB_LITE_ENGINE_ROOT}/tools/cmake/raylib_lite_native_assets.cmake")
raylib_lite_native_embed_assets("${CMAKE_CURRENT_LIST_DIR}/..")
