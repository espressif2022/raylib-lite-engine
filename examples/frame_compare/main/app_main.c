// SPDX-License-Identifier: Apache-2.0
#include "frame_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

const frame_ops_t *frame_compare_ops(void);

void app_main(void)
{
    /* Leave time for the serial monitor to attach after flash. */
    vTaskDelay(pdMS_TO_TICKS(5000));
    const frame_ops_t *ops = frame_compare_ops();
    printf("FRAMECOMPARE_BEGIN {\"path\":\"%s\",\"width\":480,\"height\":480,\"samples\":3}\n",
           ops->path);
    fflush(stdout);
    int status = frame_host_run(480, 480, 3, NULL, ops);
    printf("FRAMECOMPARE_END {\"path\":\"%s\",\"status\":%d}\n", ops->path, status);
    fflush(stdout);
}
