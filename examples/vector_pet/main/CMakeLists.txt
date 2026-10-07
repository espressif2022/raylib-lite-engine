set(RAYLIB_LITE_EXAMPLE_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../../common")
file(GLOB example_sources "${CMAKE_CURRENT_LIST_DIR}/*.c")
idf_component_register(
    SRCS ${example_sources} "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}/native_module_main.c"
    INCLUDE_DIRS "." "${RAYLIB_LITE_EXAMPLE_COMMON_DIR}"
    REQUIRES ${RAYLIB_LITE_BOARD}
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_GAME_NATIVE=1)
