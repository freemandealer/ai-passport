#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "key_control.h"
#include "key_audio.h"
#include "key_ui.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "key_radar";
static QueueHandle_t input_queue;
static key_input_t callback_input;
/* Fixed RAM: no whole-recording allocation and no retained microphone data. */
static key_workspace_t workspace;
static int16_t pcm[PITCH_FRAME_SAMPLES];
static key_session_t session;

static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user) {
    (void)user;
    if (button == BSP_BTN_OK && event == BSP_BTN_PRESS)
        key_input_apply(&callback_input, KEY_INPUT_PRESS);
    else if (button == BSP_BTN_OK && event == BSP_BTN_RELEASE)
        key_input_apply(&callback_input, KEY_INPUT_RELEASE);
    else if (button == BSP_BTN_UP && event == BSP_BTN_PRESS)
        key_input_apply(&callback_input, KEY_INPUT_MODE);
    else if (button == BSP_BTN_DOWN && event == BSP_BTN_PRESS)
        key_input_apply(&callback_input, KEY_INPUT_CLEAR);
    else return; /* CLICK/DOUBLE/LONG never start another recording. */
    xQueueOverwrite(input_queue, &callback_input);
}

static void publish(int battery) {
    if (bsp_lvgl_lock(30)) {
        key_ui_render(&session, battery, (uint32_t)(esp_timer_get_time() / 1000));
        bsp_lvgl_unlock();
    }
}

static void close_capture(bool *opened) {
    if (*opened) {
        esp_err_t error = key_audio_stop();
        if (error != ESP_OK) {
            session.phase = KEY_ERROR;
            ESP_LOGE(TAG, "Audio suspend failed: %s", esp_err_to_name(error));
        }
        *opened = false;
    }
    memset(pcm, 0, sizeof(pcm));
    memset(&workspace, 0, sizeof(workspace));
}

static void capture_task(void *arg) {
    (void)arg;
    key_input_t input = {0};
    uint32_t generation = 0;
    unsigned fill = 0, warmup = 0;
    bool opened = false;
    int battery = -1;
    bool battery_ready = bsp_battery_init() == ESP_OK;
    int64_t battery_at = 0, session_started = 0, worst_dsp_us = 0;
    int64_t last_read_end = 0, worst_read_gap_us = 0;
    while (true) {
        xQueuePeek(input_queue, &input, 0);
        if (input.generation != generation) {
            ESP_LOGI(TAG, "Input gen=%lu held=%u capture=%u mode=%u",
                     (unsigned long)input.generation, input.held, input.capture, input.mode);
            close_capture(&opened);
            generation = input.generation;
            fill = 0;
            last_read_end = worst_read_gap_us = worst_dsp_us = 0;
            if (input.capture && !input.held) {
                key_session_begin(&session, input.mode);
                key_session_finish(&session);
            } else if (input.held) {
                key_session_begin(&session, input.mode);
                publish(battery);
                session_started = esp_timer_get_time();
                esp_err_t error = key_audio_start();
                if (error == ESP_OK) {
                    opened = true;
                    /* Drain codec startup transients / up to 90 ms old DMA. */
                    warmup = PITCH_FRAME_SAMPLES;
                } else {
                    session.phase = KEY_ERROR;
                    ESP_LOGE(TAG, "Audio start failed: %s", esp_err_to_name(error));
                }
            } else {
                key_session_clear(&session, input.mode);
                ESP_LOGI(TAG, "Cleared: ready, mode=%u", input.mode);
            }
        }
        if (session.phase == KEY_LISTENING && !input.held) {
            key_session_finish(&session);
            close_capture(&opened);
            ESP_LOGI(TAG, "Result phase=%d key=%d alternative=%d frames=%u usable=%u DSP-max=%lldus read-gap=%lldus stack=%u",
                     session.phase, session.first, session.second, session.frames, session.usable,
                     (long long)worst_dsp_us, (long long)worst_read_gap_us,
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
        if (session.phase == KEY_LISTENING && opened) {
            /* 16 ms I/O pieces allow release checks during a 128 ms DSP frame. */
            int64_t now_read = esp_timer_get_time();
            if (last_read_end && now_read - last_read_end > worst_read_gap_us)
                worst_read_gap_us = now_read - last_read_end;
            esp_err_t error = bsp_audio_read(pcm + fill, 256 * sizeof(int16_t));
            last_read_end = esp_timer_get_time();
            key_input_t newest;
            xQueuePeek(input_queue, &newest, 0);
            if (error != ESP_OK) {
                session.phase = KEY_ERROR;
                ESP_LOGE(TAG, "Microphone read failed: %s", esp_err_to_name(error));
                close_capture(&opened);
            } else if (newest.generation != generation || !newest.held) {
                continue; /* Never attribute an in-flight read to the next hold. */
            } else if (warmup) {
                warmup -= 256;
            } else {
                fill += 256;
                if (fill == PITCH_FRAME_SAMPLES) {
                    int64_t start = esp_timer_get_time();
                    key_session_feed(&session, &workspace, pcm);
                    int64_t elapsed = esp_timer_get_time() - start;
                    if (elapsed > worst_dsp_us) worst_dsp_us = elapsed;
                    fill = 0;
                    xQueuePeek(input_queue, &newest, 0);
                    if (newest.generation == generation && newest.held) publish(battery);
                }
            }
            /* Wall time also bounds a slow stream, not only sample count. */
            if (esp_timer_get_time() - session_started >= 30000000) {
                session.limit_reached = true;
                key_session_finish(&session);
            }
            if (session.phase != KEY_LISTENING) close_capture(&opened);
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            int64_t now = esp_timer_get_time();
            if (battery_ready && now >= battery_at) {
                battery = bsp_battery_soc();
                battery_at = now + 15000000;
            }
            publish(battery);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(bsp_display_init());
    if (!bsp_lvgl_init() || !bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Display initialization failed"); return;
    }
    bool ui_ok = key_ui_create();
    bsp_lvgl_unlock();
    if (!ui_ok) { ESP_LOGE(TAG, "UI allocation or glyph coverage failed"); return; }
    bsp_display_backlight(70);
    input_queue = xQueueCreate(1, sizeof(key_input_t));
    if (!input_queue) { session.phase = KEY_ERROR; publish(-1); return; }
    xQueueOverwrite(input_queue, &callback_input);
    /* Queues/screens outlive the button callback and the single worker. */
    if (bsp_button_init(on_button, NULL) != ESP_OK ||
        xTaskCreate(capture_task, "key_capture", 6144, NULL, 5, NULL) != pdPASS) {
        session.phase = KEY_ERROR; publish(-1); return;
    }
    ESP_LOGI(TAG, "Ready; heap=%u largest=%u; no radios, no stored PCM",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
