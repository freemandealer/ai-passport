#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "frog_model.h"
#include "frog_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "frog";
/* A latest-state mailbox cannot lose a release when audio blocks. Sequence
 * counters retain UP/DOWN actions; only the button callback writes s_keys. */
typedef struct { bool held; uint32_t up, down, activity; } keys_t;
static QueueHandle_t s_input;
static keys_t s_keys;

static void on_key(bsp_btn_t key, bsp_btn_ev_t event, void *user) {
    (void)user;
    if (event != BSP_BTN_PRESS && event != BSP_BTN_RELEASE) return;
    if (key == BSP_BTN_OK) s_keys.held = event == BSP_BTN_PRESS;
    if (event == BSP_BTN_PRESS) {
        ++s_keys.activity;
        if (key == BSP_BTN_UP) ++s_keys.up;
        if (key == BSP_BTN_DOWN) ++s_keys.down;
    }
    (void)xQueueOverwrite(s_input, &s_keys);
}

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

/* This permanent worker serializes all PCM, codec and battery I/O. One 640-byte
 * PCM chunk is discarded after level measurement. Voice is never persisted. */
static void frog_task(void *arg) {
    (void)arg;
    frog_model_t m;
    frog_model_init(&m);
    if (bsp_button_init(on_key, NULL) != ESP_OK) m.phase = FROG_INPUT_ERROR;
    bool battery_ready = bsp_battery_init() == ESP_OK;
    int battery = battery_ready ? bsp_battery_soc() : -1;
    bool audio_active = false;
    bool audio_safe = bsp_audio_init() == ESP_OK && bsp_audio_sleep() == ESP_OK;
    uint32_t last_activity = now_ms(), battery_at = last_activity, drawn_at = 0;
    keys_t keys = {0};
    int brightness = 75;
    int16_t pcm[320];
    frog_phase_t logged = FROG_READY;
    bool dirty = true;
    for (;;) {
        uint32_t now = now_ms();
        keys_t next;
        if (xQueueReceive(s_input, &next, 0) == pdTRUE) {
            if (next.activity != keys.activity) { last_activity = now; dirty = true; }
            if (next.up != keys.up) frog_model_replay(&m, now);
            if (next.down != keys.down) frog_model_reset(&m);
            keys = next;
        }
        frog_model_input(&m, keys.held, now);
        if (m.phase == FROG_LISTENING && !audio_active) {
            bsp_display_backlight(75);
            brightness = 75;
            if (bsp_lvgl_lock(50)) {
                frog_ui_render(&m, now, battery);
                bsp_lvgl_unlock();
            }
            esp_err_t err = bsp_audio_init();
            if (err == ESP_OK) err = bsp_audio_wake();
            if (err == ESP_OK) err = bsp_audio_set_format(16000, 16, 1);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Microphone setup failed: %s", esp_err_to_name(err));
                frog_model_error(&m);
                audio_safe = bsp_audio_sleep() == ESP_OK;
            } else {
                audio_active = true;
                audio_safe = false;
            }
            // Setup may block: re-read the latest release before any PCM read.
            if (xQueueReceive(s_input, &next, 0) == pdTRUE) {
                keys = next;
                frog_model_input(&m, keys.held, now_ms());
            }
        }
        if (m.phase == FROG_LISTENING && audio_active) {
            if (bsp_audio_read(pcm, sizeof(pcm)) == ESP_OK) {
                frog_model_pcm(&m, pcm, sizeof(pcm) / sizeof(pcm[0]));
            } else {
                ESP_LOGE(TAG, "Microphone read failed; discarding round");
                frog_model_error(&m);
            }
        }
        now = now_ms();
        frog_model_tick(&m, now, esp_random());
        if (audio_active && m.phase != FROG_LISTENING) {
            // All PCM reads have returned; no other task owns this codec.
            audio_safe = bsp_audio_sleep() == ESP_OK;
            audio_active = false;
            if (!audio_safe) frog_model_error(&m);
        }
        bool moving = m.phase == FROG_LISTENING || m.phase == FROG_THINKING || m.phase == FROG_JUMPING;
        if (moving || keys.held) last_activity = now;
        if (!moving && now - battery_at >= 15000) {
            battery = battery_ready ? bsp_battery_soc() : -1;
            battery_at = now;
            dirty = true;
        }
        uint32_t idle = now - last_activity;
        int desired = idle >= 120000 ? 0 : idle >= 45000 ? 12 : 75;
        if (desired != brightness) {
            brightness = desired;
            bsp_display_backlight((uint8_t)brightness);
            dirty = true;
        }
        if (logged != m.phase) {
            ESP_LOGI(TAG, "state=%d score=%d samples=%lu free=%u largest=%u",
                     m.phase, m.score, (unsigned long)m.samples,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
            logged = m.phase;
            dirty = true;
        }
        if ((dirty || moving) && now - drawn_at >= 40 && bsp_lvgl_lock(20)) {
            frog_ui_render(&m, now, battery);
            bsp_lvgl_unlock();
            drawn_at = now;
            dirty = false;
        }
        // Reuse documented timer light sleep, without inventing GPIO wake wiring.
        // Hold OK to wake; very short taps during these 100 ms naps can be missed.
        if (brightness == 0 && audio_safe && bsp_lvgl_lock(100)) {
            esp_err_t err = esp_sleep_enable_timer_wakeup(100000);
            if (err == ESP_OK) err = esp_light_sleep_start();
            bsp_lvgl_unlock();
            if (err != ESP_OK) vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(audio_active ? 1 : 20));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Frog voice score 1.0; offline random 0..100");
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    if (!bsp_lvgl_lock(1000)) return;
    bool ready = frog_ui_create();
    bsp_lvgl_unlock();
    if (!ready) { ESP_LOGE(TAG, "UI font coverage failed"); return; }
    bsp_display_backlight(75);
    s_input = xQueueCreate(1, sizeof(keys_t));
    if (!s_input || xTaskCreate(frog_task, "frog", 6144, NULL, 4, NULL) != pdPASS) {
        if (s_input) { vQueueDelete(s_input); s_input = NULL; }
        frog_model_t m;
        frog_model_init(&m);
        m.phase = FROG_INPUT_ERROR;
        if (bsp_lvgl_lock(1000)) {
            frog_ui_render(&m, now_ms(), -1);
            bsp_lvgl_unlock();
        }
        ESP_LOGE(TAG, "Cannot allocate application task/mailbox");
    }
}
