#pragma once
/* Audio mock uses no logger, but keeps the production callsites intact. */
#define ESP_LOGE(tag, fmt, ...) ((void)0)
#define ESP_LOGI(tag, fmt, ...) ((void)0)
static inline const char *esp_err_to_name(int error)
{ (void)error; return "mock error"; }
