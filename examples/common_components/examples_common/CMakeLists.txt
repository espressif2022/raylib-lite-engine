idf_component_register(
    SRCS
        "native_module_main.c"
        "native_feedback.c"
    INCLUDE_DIRS
        "include"
    REQUIRES
        raylib-lite-engine
        esp_timer
        heap
        log)

if(DEFINED RAYLIB_LITE_EXAMPLE_TARGET_FPS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        RAYLIB_LITE_NATIVE_TARGET_FPS=${RAYLIB_LITE_EXAMPLE_TARGET_FPS})
endif()
