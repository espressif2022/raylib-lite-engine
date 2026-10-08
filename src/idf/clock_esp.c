// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_clock.h"
#include "esp_timer.h"
uint64_t raylib_lite_time_us(void)
{
    return (uint64_t)esp_timer_get_time();
}
