file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources}
    INCLUDE_DIRS "."
    REQUIRES examples_common ${RAYLIB_LITE_BOARD}
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
