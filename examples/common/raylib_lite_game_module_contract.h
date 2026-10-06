// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Shared game sources use one neutral module vocabulary. The external ELF
 * product ABI is translated only in this application-layer bridge. Host and
 * native builds never include product Runtime ABI headers. */
#if defined(MOSAICO_GAME_ELF)
#include "mosaico_game_module.h"
#include "mosaico_runtime_v1.h"

typedef mosaico_game_module_v1_t raylib_lite_game_module_v1_t;
typedef mosaico_host_input_v1_t raylib_lite_host_input_v1_t;
typedef mosaico_runtime_v1_t raylib_lite_product_runtime_v1_t;

#define RAYLIB_LITE_GAME_MODULE_ABI MOSAICO_HOST_GAME_ABI
#define RAYLIB_LITE_GAME_MODULE_EXPORT MOSAICO_GAME_MODULE_EXPORT
#define raylib_lite_game_module_v1 mosaico_game_module_v1
#define raylib_lite_product_runtime g_mosaico_rt
#define RAYLIB_LITE_HOST_INPUT_ACTION MOSAICO_HOST_INPUT_ACTION
#define RAYLIB_LITE_HOST_INPUT_POINTER MOSAICO_HOST_INPUT_POINTER
#define RAYLIB_LITE_HOST_INPUT_CONTROL MOSAICO_HOST_INPUT_CONTROL
#define RAYLIB_LITE_HOST_INPUT_IMU MOSAICO_HOST_INPUT_IMU
#define RAYLIB_LITE_HOST_CONTROL_PAUSE MOSAICO_HOST_CONTROL_PAUSE
#define RAYLIB_LITE_HOST_CONTROL_RESUME MOSAICO_HOST_CONTROL_RESUME
#define RAYLIB_LITE_HOST_CONTROL_RESET MOSAICO_HOST_CONTROL_RESET
#else
#include "raylib_lite_game_module.h"
#define RAYLIB_LITE_GAME_MODULE_ABI RAYLIB_LITE_HOST_GAME_ABI_V1
#define RAYLIB_LITE_GAME_MODULE_EXPORT
#endif
