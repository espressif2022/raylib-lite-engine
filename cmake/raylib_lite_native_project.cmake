# Mosaico device examples use this dependency resolver. Pass component paths
# explicitly; the engine does not discover repositories by sibling names.
get_filename_component(RAYLIB_LITE_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT MOSAICO_PRODUCT_ROOT AND DEFINED ENV{MOSAICO_PRODUCT_ROOT})
    set(MOSAICO_PRODUCT_ROOT "$ENV{MOSAICO_PRODUCT_ROOT}")
endif()
if(NOT MOSAICO_BSP_ROOT AND DEFINED ENV{MOSAICO_BSP_ROOT})
    set(MOSAICO_BSP_ROOT "$ENV{MOSAICO_BSP_ROOT}")
endif()
if(NOT MOSAICO_UTILS_ROOT AND DEFINED ENV{MOSAICO_UTILS_ROOT})
    set(MOSAICO_UTILS_ROOT "$ENV{MOSAICO_UTILS_ROOT}")
endif()
if(NOT MOSAICO_BOARD_PLATFORM_DIR AND MOSAICO_PRODUCT_ROOT)
    set(MOSAICO_BOARD_PLATFORM_DIR
        "${MOSAICO_PRODUCT_ROOT}/components/mosaico_board_platform")
endif()
if(NOT MOSAICO_BSP_COMPONENT_DIR AND MOSAICO_BSP_ROOT)
    set(MOSAICO_BSP_COMPONENT_DIR
        "${MOSAICO_BSP_ROOT}/components/esp-mosaico-bsp")
endif()
foreach(name MOSAICO_BOARD_PLATFORM_DIR MOSAICO_BSP_COMPONENT_DIR)
    if(NOT DEFINED ${name} AND DEFINED ENV{${name}})
        set(${name} "$ENV{${name}}")
    endif()
    if(NOT EXISTS "${${name}}/CMakeLists.txt")
        message(FATAL_ERROR
            "Set -D${name}=/path/to/component (or MOSAICO_PRODUCT_ROOT and "
            "MOSAICO_BSP_ROOT). Missing ${name}/CMakeLists.txt")
    endif()
    get_filename_component(${name} "${${name}}" ABSOLUTE)
    set(${name} "${${name}}" CACHE PATH "Mosaico component directory")
endforeach()

include("${RAYLIB_LITE_ENGINE_ROOT}/cmake/raylib_lite_esp.cmake")
mosaico_game_sdk_add_components(RAYLIB TILEMAP FX SAVE)
list(APPEND EXTRA_COMPONENT_DIRS
    "${MOSAICO_BOARD_PLATFORM_DIR}" "${MOSAICO_BSP_COMPONENT_DIR}")
if(NOT MOSAICO_AUDIO_COMPONENT_DIR AND DEFINED ENV{MOSAICO_AUDIO_COMPONENT_DIR})
    set(MOSAICO_AUDIO_COMPONENT_DIR "$ENV{MOSAICO_AUDIO_COMPONENT_DIR}")
endif()
if(NOT MOSAICO_AUDIO_PLATFORM_DIR AND DEFINED ENV{MOSAICO_AUDIO_PLATFORM_DIR})
    set(MOSAICO_AUDIO_PLATFORM_DIR "$ENV{MOSAICO_AUDIO_PLATFORM_DIR}")
endif()
if(NOT MOSAICO_AUDIO_COMPONENT_DIR AND MOSAICO_PRODUCT_ROOT)
    set(MOSAICO_AUDIO_COMPONENT_DIR "${MOSAICO_PRODUCT_ROOT}/components/mosaico_game_audio")
    set(MOSAICO_AUDIO_PLATFORM_DIR "${MOSAICO_PRODUCT_ROOT}/components/platform_esp_audio")
endif()
if(EXISTS "${MOSAICO_AUDIO_COMPONENT_DIR}/CMakeLists.txt")
    if(NOT EXISTS "${MOSAICO_AUDIO_PLATFORM_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "MOSAICO_AUDIO_COMPONENT_DIR requires MOSAICO_AUDIO_PLATFORM_DIR=/path/to/platform_esp_audio")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS
        "${MOSAICO_AUDIO_COMPONENT_DIR}" "${MOSAICO_AUDIO_PLATFORM_DIR}")
    set(MOSAICO_AUDIO_COMPONENT_DIR "${MOSAICO_AUDIO_COMPONENT_DIR}" CACHE PATH "Mosaico audio component")
    set(MOSAICO_AUDIO_PLATFORM_DIR "${MOSAICO_AUDIO_PLATFORM_DIR}" CACHE PATH "Mosaico audio platform component")
endif()

if(MOSAICO_NATIVE_IRIS)
    set(MOSAICO_RECOVERY_COMPONENT_DIR
        "${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/components/esp_mosaico_app_recovery")
    set(MOSAICO_IDF_PROJECT_CMAKE
        "${MOSAICO_UTILS_ROOT}/esp-mosaico-recovery/cmake/mosaico_idf_project.cmake")
    set(MOSAICO_SYSTEM_UPDATE_CMAKE
        "${MOSAICO_UTILS_ROOT}/mosaico-tools/cmake/system_update.cmake")
    foreach(path MOSAICO_RECOVERY_COMPONENT_DIR MOSAICO_IDF_PROJECT_CMAKE MOSAICO_SYSTEM_UPDATE_CMAKE)
        if(NOT EXISTS "${${path}}")
            message(FATAL_ERROR "Iris native requires ${path} at ${${path}}")
        endif()
    endforeach()
    if(NOT EXISTS "${MOSAICO_RECOVERY_COMPONENT_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "Iris native Recovery component is incomplete: ${MOSAICO_RECOVERY_COMPONENT_DIR}")
    endif()
    list(APPEND EXTRA_COMPONENT_DIRS
        "${MOSAICO_UTILS_ROOT}/ESP-Iris/components/esp_iris"
        "${MOSAICO_RECOVERY_COMPONENT_DIR}"
        "${RAYLIB_LITE_ENGINE_ROOT}/components/mosaico_iris_ota_size_check")
endif()
