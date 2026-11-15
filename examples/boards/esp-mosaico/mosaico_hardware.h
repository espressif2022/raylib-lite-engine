// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Single owner: the example Board lifecycle, before manager init / after deinit. */
esp_err_t mosaico_hardware_prepare(void);
esp_err_t mosaico_hardware_release(void);
esp_err_t mosaico_hardware_haptic_init(void);
esp_err_t mosaico_hardware_haptic_set(uint8_t strength);
void mosaico_hardware_haptic_deinit(void);
esp_err_t mosaico_hardware_imu_read(void *imu, float *x, float *y, float *z);

int mosaico_i2s_tx_init(void *config, int size, void **out);
int mosaico_i2s_tx_deinit(void *handle);

bool mosaico_hardware_is_v1_0(void);
int mosaico_hardware_imu_deinit(void *imu);
