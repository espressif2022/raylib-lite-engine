#pragma once
#include "FreeRTOS.h"
typedef struct fake_task *TaskHandle_t;
BaseType_t xTaskCreate(void (*task_fn)(void *), const char *name,
                        uint32_t stack_size, void *arg, uint32_t priority,
                        TaskHandle_t *out_handle);
void xTaskNotifyGive(TaskHandle_t handle);
uint32_t ulTaskNotifyTake(BaseType_t clear_on_exit, TickType_t ticks);
void vTaskSuspend(TaskHandle_t handle);
void vTaskDelete(TaskHandle_t handle);
