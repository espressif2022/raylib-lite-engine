get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(FRAME_DIR "${ENGINE_ROOT}/tools/frame_compare")
set(RAYLIB_DIR "${ENGINE_ROOT}/third_party/raylib")

set(FRAME_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/app_main.c"
    "${FRAME_DIR}/frame_host.c"
    "${FRAME_DIR}/frame_scene.c"
    "${ENGINE_ROOT}/src/runtime/raylib_lite_raylib_port.c")
set(FRAME_INCLUDES
    "${FRAME_DIR}"
    "${ENGINE_ROOT}/include/raylib_lite")
set(FRAME_DEFINITIONS "")

if(FRAME_COMPARE_IMPL STREQUAL "upstream")
    list(APPEND FRAME_SOURCES
        "${FRAME_DIR}/run_upstream.c"
        "${ENGINE_ROOT}/src/runner/raylib_lite_input_queue.c"
        "${RAYLIB_DIR}/src/rshapes.c"
        "${RAYLIB_DIR}/src/rtextures.c"
        "${RAYLIB_DIR}/src/rtext.c"
        "${ENGINE_ROOT}/src/rcore/raylib_lite_rcore.c")
    list(APPEND FRAME_INCLUDES
        "${RAYLIB_DIR}/src"
        "${RAYLIB_DIR}/src/external")
    list(APPEND FRAME_DEFINITIONS
        PLATFORM_CUSTOM
        GRAPHICS_API_OPENGL_SOFTWARE
        SW_COLOR_BUFFER_BITS=16
        SW_DEPTH_BUFFER_BITS=16)
else()
    list(APPEND FRAME_SOURCES
        "${FRAME_DIR}/run_compat.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer_raylib.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_rgb565.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_raylib_impl.c")
    list(INSERT FRAME_INCLUDES 1
        "${ENGINE_ROOT}/host/include"
        "${ENGINE_ROOT}/compat/raylib/include")
    list(APPEND FRAME_DEFINITIONS FRAME_COMPARE_COMPAT)
endif()

idf_component_register(SRCS ${FRAME_SOURCES} INCLUDE_DIRS ${FRAME_INCLUDES}
    REQUIRES esp_timer esp_hw_support esp_system esp_psram heap freertos)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${FRAME_DEFINITIONS})
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -funroll-loops)
target_link_libraries(${COMPONENT_LIB} PRIVATE m)

if(FRAME_COMPARE_IMPL STREQUAL "upstream")
    set(_upstream
        "${RAYLIB_DIR}/src/rshapes.c"
        "${RAYLIB_DIR}/src/rtextures.c"
        "${RAYLIB_DIR}/src/rtext.c"
        "${ENGINE_ROOT}/src/rcore/raylib_lite_rcore.c")
    set_property(SOURCE ${_upstream} APPEND PROPERTY COMPILE_OPTIONS
        "-I${RAYLIB_DIR}/src/external"
        -Wno-incompatible-pointer-types -Wno-uninitialized -Wno-nonnull
        -Wno-return-type -Wno-unused-variable -Wno-unused-function
        -Wno-unused-label -Wno-stringop-truncation -Wno-unused-parameter
        -Wno-missing-field-initializers -Wno-sign-compare)
else()
    set_property(SOURCE
        "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer_raylib.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_rgb565.c"
        APPEND PROPERTY COMPILE_OPTIONS
        "-include${ENGINE_ROOT}/src/renderer/raster_esp_config.h")
endif()
