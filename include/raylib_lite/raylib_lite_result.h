// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* Existing public values are explicit and must remain ABI-stable. */
    RAYLIB_LITE_OK = 0,
    RAYLIB_LITE_INVALID_ARGUMENT = 1,
    RAYLIB_LITE_INVALID_STATE = 2,
    RAYLIB_LITE_NOT_SUPPORTED = 3,
    RAYLIB_LITE_NO_MEMORY = 4,
    RAYLIB_LITE_NOT_READY = 5,
    RAYLIB_LITE_BUSY = 6,
    RAYLIB_LITE_TIMEOUT = 7,
    RAYLIB_LITE_IO_ERROR = 8,
    RAYLIB_LITE_PLATFORM_ERROR = 9,

    /* Added after the original result ABI; append new values only. */
    RAYLIB_LITE_NOT_FOUND = 10,
    RAYLIB_LITE_INVALID_SIZE = 11,
    RAYLIB_LITE_INVALID_CRC = 12,
    RAYLIB_LITE_INVALID_VERSION = 13,
} raylib_lite_result_t;

/* Use only when an operation explicitly permits an unbounded wait. */
#define RAYLIB_LITE_WAIT_FOREVER UINT32_MAX

#ifdef __cplusplus
}
#endif
