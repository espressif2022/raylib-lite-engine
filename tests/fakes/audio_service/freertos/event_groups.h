#pragma once
#include "FreeRTOS.h"
typedef uint32_t EventBits_t;
typedef struct fake_event_group *EventGroupHandle_t;
EventGroupHandle_t xEventGroupCreate(void);
void vEventGroupDelete(EventGroupHandle_t handle);
EventBits_t xEventGroupSetBits(EventGroupHandle_t handle, EventBits_t bits);
EventBits_t xEventGroupClearBits(EventGroupHandle_t handle, EventBits_t bits);
EventBits_t xEventGroupGetBits(EventGroupHandle_t handle);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t handle, EventBits_t bits,
                                 BaseType_t clear_on_exit, BaseType_t wait_all,
                                 TickType_t ticks);
