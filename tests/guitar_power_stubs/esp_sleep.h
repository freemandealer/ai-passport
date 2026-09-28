#pragma once
#include "esp_err.h"
#define ESP_SLEEP_WAKEUP_ALL 0
esp_err_t esp_sleep_disable_wakeup_source(int source);
void esp_deep_sleep_start(void);
