// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *context;

    /* Microseconds on a monotonic uint64_t timeline. It must not follow wall
     * clock corrections. Consecutive samples may wrap naturally; consumers
     * compute elapsed time with unsigned subtraction. */
    uint64_t (*monotonic_us)(void *context);

    /* Waits for at most duration_us. A zero duration returns immediately.
     * Spurious early wakeups are permitted; callers must recheck the clock. */
    void (*sleep_for_us)(void *context, uint64_t duration_us);
} raylib_lite_clock_t;

/* Platform-provided monotonic time for profiling and save debounce. This is
 * independent of the deterministic game timeline returned by GetTime().
 * ESP, Host and module-runtime adapters supply the implementation. */
uint64_t raylib_lite_time_us(void);

#ifdef __cplusplus
}
#endif
