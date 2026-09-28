// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_native_hooks.h"

__attribute__((weak)) bool raylib_lite_native_boot(void)
{
    return true;
}

__attribute__((weak)) void raylib_lite_native_first_present(void)
{
}
