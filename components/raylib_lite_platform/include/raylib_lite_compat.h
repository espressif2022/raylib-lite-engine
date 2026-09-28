// SPDX-License-Identifier: Apache-2.0
#pragma once

/* Frozen source/ELF ABI names used by the existing game API. These values do
 * not grant access to ESP-IDF services. Use the SDK definition when a product
 * provides it; standalone consumers need only an ordinary C compiler. New
 * platform contracts use raylib_lite_result_t instead. */
#if defined(__has_include)
#if __has_include("esp_err.h")
#include "esp_err.h"
#define RAYLIB_LITE_HAS_ESP_ERROR_TYPES 1
#endif
#endif

#ifndef RAYLIB_LITE_HAS_ESP_ERROR_TYPES
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_CRC 0x109
#define ESP_ERR_INVALID_VERSION 0x10A
#endif
