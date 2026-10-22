// SPDX-License-Identifier: Apache-2.0
#pragma once
/* Optional diagnostic entrypoint. Enabled implementations replace the target
 * and run startup workloads; do not call during a live drawing frame.
 * Disabled builds provide an empty operation, not a timing result. */
#ifdef __cplusplus
extern "C" {
#endif
void raylib_lite_renderer_run_benchmark(void);
#ifdef __cplusplus
}
#endif
