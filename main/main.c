#include "guitar_player.h"
#include "guitar_service.h"
#include "guitar_ui.h"
#include "guitar_audio.h"
#include "guitar_battery.h"
#include "guitar_power.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "guitar";
static guitar_library_t s_library;
static guitar_player_t s_player;
static guitar_network_info_t s_network;
static SemaphoreHandle_t s_model_lock;
static QueueHandle_t s_inputs;

/* Called only under the model mutex. The audio worker uses the same origin and
 * tempo as the visual player, and knows the song end even if rendering stalls. */
static void publish_audio(void)
{
    guitar_audio_clock_t clock = {
        .start_us = s_player.started_us,
        .bpm = s_player.bpm, .volume = s_player.volume,
        .active = s_player.transport == G_PLAYING || s_player.transport == G_COUNT_IN,
    };
    if (clock.active) {
        uint64_t bars = s_library.songs[s_player.song].bar_count - s_player.start_bar + (s_player.lead_in ? 1 : 0);
        clock.end_us = clock.start_us + (bars * UINT64_C(240000000) + clock.bpm - 1) / clock.bpm;
    }
    guitar_audio_publish(&clock);
}

/* Only the main worker and HTTP worker mutate the model. Lock order is model
 * then LVGL. HTTP storage never owns LVGL; draw events use UI-owned snapshots. */
static esp_err_t apply_score(const guitar_library_t *candidate, const char *text, size_t length)
{
    if (xSemaphoreTake(s_model_lock, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    if (guitar_power_closing()) {
        xSemaphoreGive(s_model_lock);
        return ESP_ERR_INVALID_STATE;
    }
    guitar_player_tick(&s_player, &s_library, esp_timer_get_time());
    guitar_player_pause(&s_player);
    publish_audio(); /* Stop future clicks before writing Flash. */
    esp_err_t result = guitar_service_save(text, length);
    s_network.storage_ok = result == ESP_OK;
    if (result == ESP_OK) {
        s_library = *candidate;
        uint8_t volume = s_player.volume;
        guitar_player_init(&s_player, &s_library);
        s_player.volume = volume;
        /* Discard navigation queued for the old library while Flash was busy. */
        xQueueReset(s_inputs);
    }
    xSemaphoreGive(s_model_lock);
    return result;
}

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!guitar_power_activity()) return; /* Includes PRESS before click delay. */
    guitar_action_t action;
    if (event == BSP_BTN_CLICK) action = button == BSP_BTN_UP ? G_UP : button == BSP_BTN_DOWN ? G_DOWN : G_OK;
    else if (event == BSP_BTN_DOUBLE && button == BSP_BTN_UP) action = G_UP_DOUBLE;
    else if (event == BSP_BTN_DOUBLE && button == BSP_BTN_DOWN) action = G_DOWN_DOUBLE;
    else if (event == BSP_BTN_LONG) action = button == BSP_BTN_UP ? G_UP_LONG : button == BSP_BTN_DOWN ? G_DOWN_LONG : G_OK_LONG;
    else return;
    (void)xQueueSend(s_inputs, &action, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "GREEN ROOM guitar score + audio/visual eighth-note metronome");
    s_model_lock = xSemaphoreCreateMutex();
    s_inputs = xQueueCreate(16, sizeof(guitar_action_t));
    QueueHandle_t battery_queue = xQueueCreate(1, sizeof(int));
    if (!s_model_lock || !s_inputs || !battery_queue) {
        ESP_LOGE(TAG, "Application queues unavailable");
        return;
    }
    guitar_service_load(&s_library, &s_network);
    guitar_player_init(&s_player, &s_library);
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init() || !bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Display unavailable");
        return;
    }
    bool ui_ok = guitar_ui_create() && guitar_ui_check_fonts();
    if (ui_ok) guitar_ui_render(&s_player, &s_library, -1, &s_network);
    bsp_lvgl_unlock();
    if (!ui_ok) { ESP_LOGE(TAG, "UI/font initialization failed"); return; }
    bsp_display_backlight(80);
    esp_err_t buttons = bsp_button_init(on_key, NULL);
    if (buttons != ESP_OK) ESP_LOGE(TAG, "Buttons unavailable: %s", esp_err_to_name(buttons));
    if (bsp_i2c_init() == ESP_OK && guitar_battery_start(battery_queue) != ESP_OK)
        ESP_LOGW(TAG, "Battery task unavailable");
    (void)guitar_audio_start();
    bool network_ready = guitar_service_start(apply_score, guitar_power_activity, &s_network) == ESP_OK;
    xSemaphoreTake(s_model_lock, portMAX_DELAY);
    s_network.ready = network_ready;
    xSemaphoreGive(s_model_lock);
    ESP_LOGI(TAG, "Ready; free heap=%lu, largest=%lu", (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    int battery = -1;
    vTaskPrioritySet(NULL, 5);
    /* This worker owns transport and input. No per-page tasks or timers survive
     * a screen change; the same persistent renderer presents all five pages. */
    for (;;) {
        guitar_action_t action;
        bool has_input = xQueueReceive(s_inputs, &action, pdMS_TO_TICKS(10)) == pdTRUE;
        (void)xQueueReceive(battery_queue, &battery, 0);
        if (xSemaphoreTake(s_model_lock, pdMS_TO_TICKS(20)) != pdTRUE) continue;
        uint64_t now = esp_timer_get_time();
        guitar_player_tick(&s_player, &s_library, now);
        if (has_input) guitar_player_action(&s_player, &s_library, action, now);
        bool playing = s_player.transport == G_PLAYING || s_player.transport == G_COUNT_IN;
        if (guitar_power_idle_due(playing, now)) {
            guitar_player_pause(&s_player);
            publish_audio();
            xSemaphoreGive(s_model_lock);
            guitar_power_off();
            return;
        }
        publish_audio();
        s_network.audio_failed = guitar_audio_status() < 0;
        if (bsp_lvgl_lock(15)) {
            guitar_ui_render(&s_player, &s_library, battery, &s_network);
            bsp_lvgl_unlock();
        }
        xSemaphoreGive(s_model_lock);
    }
}
