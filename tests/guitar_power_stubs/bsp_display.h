#pragma once
#include "esp_err.h"
#include <stdbool.h>
bool bsp_lvgl_lock(int timeout_ms);
esp_err_t bsp_display_prepare_deep_sleep(void);
