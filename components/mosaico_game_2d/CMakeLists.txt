# Wall raster experiments; definitions apply to the renderer and benchmark.
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
set(srcs "mosaico_game_2d_esp.c" "mosaico_game_2d_raylib.c"
    "mosaico_rgb565.c" "mosaico_mtx2.c" "mosaico_raster_bench.c")
if(IDF_TARGET STREQUAL "esp32s31")
    list(APPEND srcs "arch/esp32s31/mosaico_rgb565_pie.S")
endif()
if(M2D_WALL_BENCHMARK)
    list(APPEND srcs "mosaico_wall_bench.c")
endif()
idf_component_register(SRCS ${srcs} INCLUDE_DIRS "include"
    REQUIRES mosaico_game_assets raylib PRIV_REQUIRES esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -funroll-loops
    "$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_LIST_DIR}/raster_esp_config.h>")
target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
if(IDF_TARGET STREQUAL "esp32s31")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MOSAICO_RGB565_PIE=1)
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    M2D_WALL_MODE=${M2D_WALL_MODE}
    M2D_WALL_FIXED_PIXELS=${M2D_WALL_FIXED_PIXELS}
    M2D_WALL_ERROR_TEXELS=${M2D_WALL_ERROR_TEXELS}f)
if(M2D_WALL_BENCHMARK)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE M2D_WALL_BENCHMARK=1)
endif()
if(M2D_WALL_AUDIT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE M2D_WALL_AUDIT=1)
endif()
