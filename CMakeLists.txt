# Raylib Lite Engine single ESP-IDF component definition.
# IDF derives the component name from this repository directory basename, so
# consumers depend on the root component as `raylib-lite-engine`.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)

set(M2D_WALL_MODE "0" CACHE STRING "0 legacy, 1 exact, 2 fixed, 3 error bounded")
set(M2D_WALL_FIXED_PIXELS "16" CACHE STRING "Fixed perspective segment length")
set(M2D_WALL_ERROR_TEXELS "0.25" CACHE STRING "Adaptive UV error budget")
if(M2D_WALL_ERROR_TEXELS LESS_EQUAL 0 OR M2D_WALL_ERROR_TEXELS GREATER 1)
    message(FATAL_ERROR "M2D_WALL_ERROR_TEXELS must be in (0,1]")
endif()
option(M2D_WALL_BENCHMARK "Run deterministic wall microbenchmark at raster benchmark entry" OFF)
option(M2D_WALL_AUDIT "Audit wall UV (never use audit timings for acceptance)" OFF)
if(M2D_WALL_AUDIT AND NOT M2D_WALL_BENCHMARK)
    message(FATAL_ERROR "M2D_WALL_AUDIT requires M2D_WALL_BENCHMARK")
endif()

set(_platform_sources "${RAYLIB_LITE_ENGINE_ROOT}/src/idf/clock_esp.c")
set(_runner_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runner/raylib_lite_runner.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runner/raylib_lite_input_queue.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runner/raylib_lite_runtime_stats.c")
set(_runtime_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runtime/mosaico_game.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/input/mosaico_game_action.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runtime/mosaico_game_debug.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runtime/mosaico_game_app.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runtime/raylib_lite_native_hooks.c")
set(_asset_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/assets/mosaico_game_assets.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/idf/mosaico_game_assets_mmap.c")
set(_audio_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/audio/raylib_lite_audio_decode.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/audio/raylib_lite_audio_mixer.c")
set(_renderer_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d_esp.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d_raylib.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_rgb565.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_mtx2.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_raster_bench.c")
if(IDF_TARGET STREQUAL "esp32s31")
    list(APPEND _renderer_sources
        "${RAYLIB_LITE_ENGINE_ROOT}/src/arch/esp32s31/mosaico_rgb565_pie.S")
endif()
if(M2D_WALL_BENCHMARK)
    list(APPEND _renderer_sources
        "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_wall_bench.c")
endif()
set(_raylib_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/runtime/mosaico_raylib_port.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_raylib_fast.c")
set(_feature_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_tilemap.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/scene/mosaico_game_scene.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/ui/mosaico_game_ui.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/fx/mosaico_game_fx.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/save/mosaico_game_save.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/idf/mosaico_game_save_nvs.c")

idf_component_register(
    SRCS ${_platform_sources} ${_runner_sources} ${_runtime_sources}
         ${_asset_sources} ${_audio_sources} ${_renderer_sources}
         ${_raylib_sources} ${_feature_sources}
    INCLUDE_DIRS
        "${RAYLIB_LITE_ENGINE_ROOT}/include/raylib_lite"
        "${RAYLIB_LITE_ENGINE_ROOT}/compat/raylib/include"
    PRIV_INCLUDE_DIRS
        "${RAYLIB_LITE_ENGINE_ROOT}/src/assets"
        "${RAYLIB_LITE_ENGINE_ROOT}/src/audio"
        "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer"
    REQUIRES raylib
    PRIV_REQUIRES esp_timer esp_system heap freertos esp_mmap_assets
                  nvs_flash log)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
    MOSAICO_GAME_DEFAULT_FPS=${CONFIG_MOSAICO_GAME_DEFAULT_FPS})

set(_renderer_c_sources
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d_esp.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_game_2d_raylib.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_rgb565.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_mtx2.c"
    "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_raster_bench.c")
if(M2D_WALL_BENCHMARK)
    list(APPEND _renderer_c_sources
        "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_wall_bench.c")
endif()
foreach(_source IN LISTS _renderer_c_sources)
    set_property(SOURCE "${_source}" APPEND PROPERTY COMPILE_OPTIONS
        -O3 -funroll-loops
        "-include${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/raster_esp_config.h")
    set_property(SOURCE "${_source}" APPEND PROPERTY COMPILE_DEFINITIONS
        M2D_WALL_MODE=${M2D_WALL_MODE}
        M2D_WALL_FIXED_PIXELS=${M2D_WALL_FIXED_PIXELS}
        M2D_WALL_ERROR_TEXELS=${M2D_WALL_ERROR_TEXELS}f)
endforeach()
if(IDF_TARGET STREQUAL "esp32s31")
    set_property(SOURCE
        "${RAYLIB_LITE_ENGINE_ROOT}/src/renderer/mosaico_rgb565.c"
        APPEND PROPERTY COMPILE_DEFINITIONS MOSAICO_RGB565_PIE=1)
endif()
if(M2D_WALL_BENCHMARK)
    foreach(_source IN LISTS _renderer_c_sources)
        set_property(SOURCE "${_source}" APPEND PROPERTY COMPILE_DEFINITIONS
            M2D_WALL_BENCHMARK=1)
    endforeach()
endif()
if(M2D_WALL_AUDIT)
    foreach(_source IN LISTS _renderer_c_sources)
        set_property(SOURCE "${_source}" APPEND PROPERTY COMPILE_DEFINITIONS
            M2D_WALL_AUDIT=1)
    endforeach()
endif()
