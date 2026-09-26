#include "guitar_battery.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>

static const char *TAG = "guitar_battery";

void guitar_battery_worker(void *arg)
{
    bool ready = false, unavailable = false;
    for (;;) {
        /* A cold gauge or temporary bus error must not disable SOC for the
         * entire session. The BSP releases failed initialization resources. */
        if (!ready) ready = bsp_battery_init() == ESP_OK;
        int battery = ready ? bsp_battery_soc() : -1;
        if (battery < 0 || battery > 100) battery = -1;
        if (battery < 0 && !unavailable) ESP_LOGW(TAG, "Battery unavailable; retrying in 5 s");
        if (battery >= 0 && unavailable) ESP_LOGI(TAG, "Battery readings recovered");
        unavailable = battery < 0;
        xQueueOverwrite((QueueHandle_t)arg, &battery);
        /* No waits, I2C, or recovery work run in the UI/transport task. */
        vTaskDelay(pdMS_TO_TICKS(unavailable ? 5000 : 15000));
    }
}
