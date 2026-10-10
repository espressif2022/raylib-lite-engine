/* SPDX-License-Identifier: Apache-2.0 */
/* ESP32-S3 PIE: 8 RGB565 pixels per 128-bit store.
 * ee.vld.128.ip and ee.vst.128.ip fault unless the address is 16-byte
 * aligned, so a short scalar head aligns the destination. An unaligned
 * source is shifted once per byte with ee.ld.128.usar.ip / ee.src.q.ld.ip.
 * Count is pixels. Only a2–a7 are used. */
#include "sdkconfig.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)

    .text
    .balign 4
    .global raylib_lite_rgb565_copy_pie
    .type raylib_lite_rgb565_copy_pie, @function
raylib_lite_rgb565_copy_pie:
    /* a2=dst, a3=src, a4=count (pixels) */
    entry   a1, 32
    beqz    a4, .Lcopy_done
    slli    a4, a4, 1
    movi    a5, 15
    and     a6, a2, a5
    beqz    a6, .Lcopy_dest_aligned
    movi    a7, 16
    sub     a6, a7, a6
    sub     a4, a4, a6
.Lcopy_peel:
    l16ui   a7, a3, 0
    s16i    a7, a2, 0
    addi    a3, a3, 2
    addi    a2, a2, 2
    addi    a6, a6, -2
    bnez    a6, .Lcopy_peel
.Lcopy_dest_aligned:
    movi    a5, 15
    and     a5, a3, a5
    bnez    a5, .Lcopy_unaligned
    extui   a6, a4, 0, 4
    srli    a5, a4, 4
    beqz    a5, .Lcopy_tail
    loopnez a5, .Lcopy_aligned_end
    ee.vld.128.ip q0, a3, 16
    ee.vst.128.ip q0, a2, 16
.Lcopy_aligned_end:
    j       .Lcopy_tail
.Lcopy_unaligned:
    /* 48-byte shifted body, then 32/16 from the same pipeline.
     * The shift reads at most 16 bytes past the requested length. */
    movi    a5, 0
    movi    a7, 48
    mov     a6, a4
.Lcopy_div:
    bltu    a6, a7, .Lcopy_div_done
    sub     a6, a6, a7
    addi    a5, a5, 1
    j       .Lcopy_div
.Lcopy_div_done:
    ee.ld.128.usar.ip q0, a3, 16
    ee.ld.128.usar.ip q1, a3, 16
    beqz    a5, .Lcopy_after_pipe
    loopnez a5, .Lcopy_pipe_end
    ee.src.q.ld.ip q2, a3, 16, q0, q1
    ee.vst.128.ip q0, a2, 16
    ee.src.q.ld.ip q0, a3, 16, q1, q2
    ee.vst.128.ip q1, a2, 16
    ee.src.q.ld.ip q1, a3, 16, q2, q0
    ee.vst.128.ip q2, a2, 16
.Lcopy_pipe_end:
.Lcopy_after_pipe:
    bbci    a6, 5, .Lcopy_mod16
    ee.src.q.ld.ip q2, a3, 0, q0, q1
    ee.vst.128.ip q0, a2, 16
    ee.src.q    q1, q1, q2
    ee.vst.128.ip q1, a2, 16
    j       .Lcopy_mask_tail
.Lcopy_mod16:
    bbci    a6, 4, .Lcopy_rewind
    ee.src.q    q0, q0, q1
    ee.vst.128.ip q0, a2, 16
    addi    a3, a3, -16
    j       .Lcopy_mask_tail
.Lcopy_rewind:
    addi    a3, a3, -32
.Lcopy_mask_tail:
    extui   a6, a6, 0, 4
.Lcopy_tail:
    beqz    a6, .Lcopy_done
    srli    a6, a6, 1
.Lcopy_tail_loop:
    l16ui   a7, a3, 0
    s16i    a7, a2, 0
    addi    a3, a3, 2
    addi    a2, a2, 2
    addi    a6, a6, -1
    bnez    a6, .Lcopy_tail_loop
.Lcopy_done:
    retw
    .size raylib_lite_rgb565_copy_pie, .-raylib_lite_rgb565_copy_pie

    .balign 4
    .global raylib_lite_rgb565_fill_pie
    .type raylib_lite_rgb565_fill_pie, @function
raylib_lite_rgb565_fill_pie:
    /* a2=dst, a3=color, a4=count (pixels) */
    entry   a1, 32
    beqz    a4, .Lfill_done
    extui   a6, a3, 0, 16
    slli    a7, a6, 16
    or      a6, a6, a7
    ee.movi.32.q q0, a6, 0
    ee.movi.32.q q0, a6, 1
    ee.movi.32.q q0, a6, 2
    ee.movi.32.q q0, a6, 3
    movi    a5, 15
    and     a7, a2, a5
    beqz    a7, .Lfill_aligned
    movi    a5, 16
    sub     a7, a5, a7
    srli    a7, a7, 1
.Lfill_peel:
    s16i    a3, a2, 0
    addi    a2, a2, 2
    addi    a4, a4, -1
    addi    a7, a7, -1
    bnez    a7, .Lfill_peel
.Lfill_aligned:
    extui   a7, a4, 0, 3
    srli    a5, a4, 3
    beqz    a5, .Lfill_tail
    loopnez a5, .Lfill_loop_end
    ee.vst.128.ip q0, a2, 16
.Lfill_loop_end:
.Lfill_tail:
    beqz    a7, .Lfill_done
    s16i    a3, a2, 0
    addi    a2, a2, 2
    addi    a7, a7, -1
    j       .Lfill_tail
.Lfill_done:
    retw
    .size raylib_lite_rgb565_fill_pie, .-raylib_lite_rgb565_fill_pie

#endif
