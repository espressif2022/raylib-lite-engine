#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct esp_display_presenter esp_display_presenter_t;
typedef struct { void *pixels; unsigned pixel_format; } fake_surface_t;
typedef esp_err_t (*esp_display_resolve_rows_fn)(void *, uint32_t, size_t, size_t, size_t *);
typedef struct {
    fake_surface_t surface;
    size_t capacity_bytes;
    uint32_t lease_id;
    esp_display_resolve_rows_fn resolve_rows;
    void *resolve_rows_ctx;
} esp_display_presenter_buffer_t;
typedef struct { int32_t x1, y1, x2, y2; } esp_display_present_area_t;
typedef struct { unsigned coverage; } esp_display_presenter_submit_t;
#define ESP_DISPLAY_PRESENT_COVERAGE_FULL 1U
esp_err_t esp_display_presenter_begin_next_frame(esp_display_presenter_t *, const void *, void *, size_t, size_t *, bool *);
esp_err_t esp_display_presenter_acquire_buffer(esp_display_presenter_t *, esp_display_presenter_buffer_t *);
esp_err_t esp_display_presenter_submit_buffer(esp_display_presenter_t *, esp_display_presenter_buffer_t *, const esp_display_present_area_t *, size_t);
void esp_display_presenter_cancel_frame(esp_display_presenter_t *);
esp_err_t esp_display_presenter_commit_frame(esp_display_presenter_t *, const esp_display_presenter_submit_t *);
esp_err_t esp_display_presenter_quiesce(esp_display_presenter_t *, uint32_t);
