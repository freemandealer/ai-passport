#pragma once
#include "FreeRTOS.h"
QueueHandle_t xQueueCreate(unsigned count, unsigned size);
int xQueueOverwrite(QueueHandle_t queue, const void *item);
int xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait);
void vQueueDelete(QueueHandle_t queue);
