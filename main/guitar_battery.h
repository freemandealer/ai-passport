#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

/* Lifetime worker; mailbox is a one-slot queue of int percentages. Shared I2C is
 * initialized by app_main before starting device workers. No LVGL access. */
esp_err_t guitar_battery_start(QueueHandle_t mailbox);
void guitar_battery_request_stop(void);
bool guitar_battery_stopped(void);
