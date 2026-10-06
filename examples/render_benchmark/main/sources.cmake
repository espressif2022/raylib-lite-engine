get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(BENCH_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_main.c"
    "${CMAKE_CURRENT_LIST_DIR}/benchmark_assets.c"
    "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer.c"
    "${ENGINE_ROOT}/src/renderer/raylib_lite_renderer_raylib.c"
    "${ENGINE_ROOT}/src/renderer/raylib_lite_rgb565.c")
set(BENCH_INCLUDES
    "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${ENGINE_ROOT}/include/raylib_lite"
    "${ENGINE_ROOT}/compat/raylib/include")
set(BENCH_DEFINITIONS
    M2D_WALL_MODE=${M2D_WALL_MODE}
    M2D_WALL_FIXED_PIXELS=${M2D_WALL_FIXED_PIXELS}
    M2D_WALL_ERROR_TEXELS=${M2D_WALL_ERROR_TEXELS}f)
if(RENDER_BENCH_DISPLAY)
    list(APPEND BENCH_SOURCES "${CMAKE_CURRENT_LIST_DIR}/render_preview.c"
        "${CMAKE_CURRENT_LIST_DIR}/core_bench.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_mtx2.c")
    if(NOT RENDER_BENCH_HOST)
        list(APPEND BENCH_SOURCES "${CMAKE_CURRENT_LIST_DIR}/render_display_esp.c")
    endif()
    list(APPEND BENCH_DEFINITIONS RENDER_BENCH_DISPLAY=1
        RENDER_BENCH_PREVIEW_FRAMES=${RENDER_BENCH_PREVIEW_FRAMES})
elseif(RENDER_BENCH_SUITE STREQUAL "wall")
    list(APPEND BENCH_SOURCES "${ENGINE_ROOT}/src/renderer/raylib_lite_wall_bench.c")
    list(APPEND BENCH_DEFINITIONS RENDER_BENCH_WALL=1)
else()
    list(APPEND BENCH_SOURCES "${CMAKE_CURRENT_LIST_DIR}/core_bench.c"
        "${ENGINE_ROOT}/src/renderer/raylib_lite_mtx2.c")
endif()
if(M2D_WALL_AUDIT)
    list(APPEND BENCH_DEFINITIONS M2D_WALL_AUDIT=1)
endif()
if(RENDER_BENCH_LUT_INTERNAL)
    list(APPEND BENCH_DEFINITIONS M2D_BENCH_LUT_INTERNAL=1)
endif()
if(RENDER_BENCH_PIE)
    if(RENDER_BENCH_HOST OR NOT IDF_TARGET STREQUAL "esp32s31")
        message(FATAL_ERROR "PIE requires ESP32-S31; host must stay scalar")
    endif()
    list(APPEND BENCH_SOURCES "${ENGINE_ROOT}/src/arch/esp32s31/raylib_lite_rgb565_pie.S")
    list(APPEND BENCH_DEFINITIONS RAYLIB_LITE_RGB565_PIE=1)
endif()

if(RENDER_BENCH_DISPLAY)
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/render_preview.c" preview_sha)
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/core_bench.c" core_preview_sha)
    string(SHA256 workload_sha "${preview_sha}:${core_preview_sha}")
elseif(RENDER_BENCH_SUITE STREQUAL "wall")
    file(SHA256 "${ENGINE_ROOT}/src/renderer/raylib_lite_wall_bench.c" workload_sha)
else()
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/core_bench.c" workload_sha)
endif()
list(APPEND BENCH_DEFINITIONS RENDER_BENCH_WORKLOAD_SHA256="${workload_sha}")
