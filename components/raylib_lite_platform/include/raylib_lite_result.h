// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RAYLIB_LITE_OK = 0,
    RAYLIB_LITE_INVALID_ARGUMENT,
    RAYLIB_LITE_INVALID_STATE,
    RAYLIB_LITE_NOT_SUPPORTED,
    RAYLIB_LITE_NO_MEMORY,
    RAYLIB_LITE_NOT_READY,
    RAYLIB_LITE_BUSY,
    RAYLIB_LITE_TIMEOUT,
    RAYLIB_LITE_IO_ERROR,
    RAYLIB_LITE_PLATFORM_ERROR,
} raylib_lite_result_t;

/* Use only when an operation explicitly permits an unbounded wait. */
#define RAYLIB_LITE_WAIT_FOREVER UINT32_MAX

#ifdef __cplusplus
}
#endif
