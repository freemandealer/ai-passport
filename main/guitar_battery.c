#include "guitar_battery.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdatomic.h>

static const char *TAG = "guitar_battery";
static atomic_bool s_stop_requested, s_stopped = true;
static bool s_started;

void guitar_battery_request_stop(void) { atomic_store(&s_stop_requested, true); }
bool guitar_battery_stopped(void) { return atomic_load(&s_stopped); }

static void worker(void *arg)
{
    bool ready = false, unavailable = false;
    while (!atomic_load(&s_stop_requested)) {
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
        for (unsigned ms = unavailable ? 5000 : 15000; ms && !atomic_load(&s_stop_requested); ms -= 100)
            vTaskDelay(pdMS_TO_TICKS(100));
    }
    atomic_store(&s_stopped, true);
    vTaskDelete(NULL);
}

esp_err_t guitar_battery_start(QueueHandle_t mailbox)
{
    if (s_started) return ESP_ERR_INVALID_STATE;
    atomic_store(&s_stop_requested, false);
    atomic_store(&s_stopped, false);
    if (xTaskCreate(worker, "guitar_battery", 3072, mailbox, 2, NULL) != pdPASS) {
        atomic_store(&s_stopped, true);
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    return ESP_OK;
}
