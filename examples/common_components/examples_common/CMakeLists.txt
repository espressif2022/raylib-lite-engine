set(_native_services)
if(RAYLIB_LITE_NATIVE_SERVICE_COMPONENT)
    list(APPEND _native_services "${RAYLIB_LITE_NATIVE_SERVICE_COMPONENT}")
endif()
idf_component_register(
    SRCS
        "native_module_main.c"
        "native_feedback.c"
        "example_async_video.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        raylib-lite-engine
        examples_audio
        esp_timer
        heap
        log
    PRIV_REQUIRES freertos ${_native_services})

if(DEFINED RAYLIB_LITE_EXAMPLE_TARGET_FPS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        RAYLIB_LITE_NATIVE_TARGET_FPS=${RAYLIB_LITE_EXAMPLE_TARGET_FPS})
endif()

if(RAYLIB_LITE_NATIVE_SERVICE_COMPONENT)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_NATIVE_SERVICES_ENABLED=1)
endif()

if(DEFINED RAYLIB_LITE_EXAMPLE_STATS_SECONDS)
    if(RAYLIB_LITE_EXAMPLE_STATS_SECONDS LESS 1)
        message(FATAL_ERROR "RAYLIB_LITE_EXAMPLE_STATS_SECONDS must be positive")
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        RAYLIB_LITE_NATIVE_STATS_SECONDS=${RAYLIB_LITE_EXAMPLE_STATS_SECONDS})
endif()

# Replay is a build-local instrumentation input, never a production default.
if(RAYLIB_LITE_EXAMPLE_REPLAY)
    get_filename_component(_native_replay "${RAYLIB_LITE_EXAMPLE_REPLAY}" ABSOLUTE
                           BASE_DIR "${CMAKE_SOURCE_DIR}")
    if(NOT EXISTS "${_native_replay}" OR NOT _native_replay MATCHES "\\.c$")
        message(FATAL_ERROR "RAYLIB_LITE_EXAMPLE_REPLAY must name generated replay C")
    endif()
    target_sources(${COMPONENT_LIB} PRIVATE "${_native_replay}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE RAYLIB_LITE_NATIVE_REPLAY_ENABLED=1)
endif()
