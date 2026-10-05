include("${CMAKE_CURRENT_LIST_DIR}/sources.cmake")
idf_component_register(SRCS ${BENCH_SOURCES} INCLUDE_DIRS ${BENCH_INCLUDES}
    REQUIRES esp_timer esp_hw_support esp_system esp_psram esp_app_format heap freertos nvs_flash)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${BENCH_DEFINITIONS})
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -funroll-loops
    "$<$<COMPILE_LANGUAGE:C>:-include${ENGINE_ROOT}/src/renderer/raster_esp_config.h>")

if(RENDER_BENCH_DISPLAY)
    # BSP is explicitly included in top-level COMPONENTS for early dependency
    # discovery; link its public headers/libraries in the normal CMake pass.
    target_link_libraries(${COMPONENT_LIB} PRIVATE idf::esp-mosaico-bsp)
endif()
