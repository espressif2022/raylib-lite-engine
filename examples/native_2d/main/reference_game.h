// SPDX-License-Identifier: Apache-2.0
#pragma once

/* The launcher has configured the platform. It flushes and closes the
 * window after this run returns. screenshot can be NULL. */
int native_2d_run(const char *screenshot);
