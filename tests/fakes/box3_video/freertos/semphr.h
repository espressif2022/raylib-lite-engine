#pragma once
#include "FreeRTOS.h"

typedef struct fake_sem *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *woken);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);
void vSemaphoreDelete(SemaphoreHandle_t sem);
