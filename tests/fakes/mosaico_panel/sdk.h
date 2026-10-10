// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
typedef int esp_err_t;
typedef void *esp_lcd_panel_io_handle_t;
typedef struct esp_lcd_panel_t esp_lcd_panel_t;
typedef esp_lcd_panel_t *esp_lcd_panel_handle_t;
struct esp_lcd_panel_t {
    esp_err_t (*reset)(esp_lcd_panel_t *);
    esp_err_t (*init)(esp_lcd_panel_t *);
    esp_err_t (*disp_on_off)(esp_lcd_panel_t *, bool);
    esp_err_t (*del)(esp_lcd_panel_t *);
};
typedef struct {
    int reset_gpio_num;
    void *vendor_config;
} esp_lcd_panel_dev_config_t;
typedef struct {
    uint8_t cmd;
    const void *data;
    size_t data_bytes;
    unsigned delay_ms;
} co5300_lcd_init_cmd_t;
typedef struct {
    const co5300_lcd_init_cmd_t *init_cmds;
    size_t init_cmds_size;
    struct { bool use_qspi_interface; } flags;
} co5300_vendor_config_t;
typedef void *esp_lcd_touch_handle_t;
typedef struct { int dummy; } esp_lcd_touch_config_t;
extern uint32_t mock_handoff;
#define LP_SYSTEM_REG_LP_STORE15_REG 15
#define REG_READ(reg) ((void)(reg), mock_handoff)
#define REG_WRITE(reg, value) ((void)(reg), mock_handoff = (value))
esp_err_t esp_lcd_new_panel_co5300(esp_lcd_panel_io_handle_t,
    const esp_lcd_panel_dev_config_t *, esp_lcd_panel_handle_t *);
esp_err_t esp_lcd_panel_co5300_set_brightness(esp_lcd_panel_handle_t, uint8_t);
esp_err_t esp_lcd_panel_io_tx_param(esp_lcd_panel_io_handle_t, int, const void *, size_t);
esp_err_t esp_lcd_touch_new_i2c_cst9220(esp_lcd_panel_io_handle_t,
    const esp_lcd_touch_config_t *, esp_lcd_touch_handle_t *);
