// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Native firmware lifecycle hooks. The engine's native entry points provide
 * weak no-op definitions; product firmware overrides them by linking a
 * component that defines these symbols (register it with WHOLE_ARCHIVE).
 *
 * raylib_lite_native_boot() runs first in app_main, before the board is
 * created. Returning false aborts startup.
 * raylib_lite_native_first_present() runs once, after the first frame has
 * been presented and board input has started.
 */
bool raylib_lite_native_boot(void);
void raylib_lite_native_first_present(void);

#ifdef __cplusplus
}
#endif
