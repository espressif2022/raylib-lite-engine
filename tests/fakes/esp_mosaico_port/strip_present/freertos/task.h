#pragma once
typedef void *TaskHandle_t;
int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name,
                            unsigned stack, void *arg, unsigned priority,
                            TaskHandle_t *handle, int core);
void vTaskDelete(TaskHandle_t task);
