#pragma once
#include <stdbool.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL (-1)

typedef void *esp_lcd_panel_io_handle_t;
typedef struct {
    unsigned reserved;
} esp_lcd_panel_io_event_data_t;
typedef struct {
    bool (*on_color_trans_done)(esp_lcd_panel_io_handle_t io,
                                esp_lcd_panel_io_event_data_t *event,
                                void *user_ctx);
} esp_lcd_panel_io_callbacks_t;

esp_err_t esp_lcd_panel_io_register_event_callbacks(
    esp_lcd_panel_io_handle_t io, const esp_lcd_panel_io_callbacks_t *callbacks,
    void *user_ctx);
