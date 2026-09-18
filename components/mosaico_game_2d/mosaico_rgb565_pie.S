/* SPDX-License-Identifier: Apache-2.0 */
/* ESP32-S31 / P4-class PIE: 8 RGB565 pixels per 128-bit load/store.
 * PIE-capable ARs only: a0-a5, t3-t6. Count is pixels and a multiple of 8. */
#include "sdkconfig.h"
#if defined(CONFIG_IDF_TARGET_ESP32S31)

    .option norvc
    .text
    .balign 4
    .global mosaico_rgb565_copy_pie
    .type mosaico_rgb565_copy_pie, @function
mosaico_rgb565_copy_pie:
    /* a0=dst, a1=src, a2=count */
    beqz    a2, 1f
    esp.movx.r.cfg t6
    ori     t6, t6, 3
    esp.movx.w.cfg t6
    mv      t3, a0
    mv      t4, a1
    srli    t5, a2, 3
    beqz    t5, 1f
0:
    esp.vld.128.ip q0, t4, 16
    addi    t5, t5, -1
    esp.vst.128.ip q0, t3, 16
    bnez    t5, 0b
1:
    ret
    .size mosaico_rgb565_copy_pie, .-mosaico_rgb565_copy_pie

    .balign 4
    .global mosaico_rgb565_fill_pie
    .type mosaico_rgb565_fill_pie, @function
mosaico_rgb565_fill_pie:
    /* a0=dst, a1=color, a2=count */
    beqz    a2, 1f
    addi    sp, sp, -16
    sh      a1, 0(sp)
    esp.movx.r.cfg t6
    ori     t6, t6, 3
    esp.movx.w.cfg t6
    mv      t4, sp
    esp.vldbc.16.ip q0, t4, 0
    mv      t3, a0
    srli    t5, a2, 3
    beqz    t5, 2f
0:
    addi    t5, t5, -1
    esp.vst.128.ip q0, t3, 16
    bnez    t5, 0b
2:
    addi    sp, sp, 16
1:
    ret
    .size mosaico_rgb565_fill_pie, .-mosaico_rgb565_fill_pie

#endif
