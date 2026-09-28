#include "guitar_power.h"
#include "guitar_idle.h"
#include "guitar_audio.h"
#include "guitar_battery.h"
#include "guitar_service.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

#define CLOSING (1u << 31)
static atomic_uint s_activity;
static guitar_idle_t s_idle;
static const char *TAG = "guitar_power";

bool guitar_power_activity(void)
{
    unsigned observed = atomic_load(&s_activity);
    do {
        if (observed & CLOSING) return false;
    } while (!atomic_compare_exchange_weak(&s_activity, &observed, (observed + 1) & ~CLOSING));
    return true;
}

bool guitar_power_closing(void) { return (atomic_load(&s_activity) & CLOSING) != 0; }

bool guitar_power_idle_due(bool playing, uint64_t now)
{
    unsigned observed = atomic_load(&s_activity);
    if (observed & CLOSING) return false;
    if (!guitar_idle_update(&s_idle, playing, observed, now)) return false;
    return atomic_compare_exchange_strong(&s_activity, &observed, observed | CLOSING);
}

static void warn(const char *step, esp_err_t err)
{
    (void)step;
    if (err != ESP_OK) ESP_LOGW(TAG, "%s: %s", step, esp_err_to_name(err));
}

void guitar_power_off(void)
{
    ESP_LOGI(TAG, "Five minutes idle; entering deep sleep (power cycle to restart)");
    /* Drain HTTP outside the model mutex: an in-flight save may need it. */
    if (guitar_service_stop() != ESP_OK) {
        ESP_LOGE(TAG, "HTTP/Wi-Fi stop failed; restarting safely");
        esp_restart();
        return;
    }
    guitar_audio_request_stop();
    guitar_battery_request_stop();
    uint64_t deadline = esp_timer_get_time() + UINT64_C(8000000);
    while (!guitar_audio_stopped() || !guitar_battery_stopped()) {
        if ((uint64_t)esp_timer_get_time() >= deadline) {
            ESP_LOGE(TAG, "Worker stop timed out; restarting before releasing hardware");
            esp_restart();
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    /* No timed or unverified ADC-button wakeup. The independent hardware
     * power button is used to power-cycle the sleeping device. */
    warn("Disable wake sources", esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL));
    warn("CW2017 suspend", bsp_battery_sleep());
    warn("ES8311 suspend", bsp_audio_sleep());
    warn("I2S pin release", bsp_audio_prepare_deep_sleep());
    warn("I2C pin release", bsp_i2c_prepare_deep_sleep());
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Cannot stop display refresh; restarting");
        esp_restart();
        return;
    }
    warn("LCD suspend", bsp_display_prepare_deep_sleep());
    esp_deep_sleep_start();
    /* Terminal BSP preparation cannot be reversed in this boot. */
    esp_restart();
}
