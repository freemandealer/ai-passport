#include "guitar_audio.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

static const char *TAG = "guitar_audio";
static QueueHandle_t s_mailbox;
static atomic_int s_status;
static atomic_bool s_stop_requested, s_stopped = true;
static guitar_audio_clock_t s_published;

void guitar_audio_publish(const guitar_audio_clock_t *clock)
{
    if (!s_mailbox || atomic_load(&s_stop_requested)) return;
    if (clock->active == s_published.active && clock->start_us == s_published.start_us &&
        clock->end_us == s_published.end_us && clock->volume == s_published.volume && clock->bpm == s_published.bpm) return;
    s_published = *clock;
    xQueueOverwrite(s_mailbox, clock);
}

int guitar_audio_status(void) { return atomic_load(&s_status); }
void guitar_audio_request_stop(void) { atomic_store(&s_stop_requested, true); }
bool guitar_audio_stopped(void) { return atomic_load(&s_stopped); }

static void worker(void *arg)
{
    (void)arg;
    esp_err_t err = bsp_audio_init();
    if (err == ESP_OK) err = bsp_audio_set_format(GUITAR_AUDIO_RATE, 16, 1);
    if (err == ESP_OK) {
        bsp_audio_set_volume(75);
        atomic_store(&s_status, 1);
        ESP_LOGI(TAG, "Eighth-note audio ready");
        guitar_audio_clock_t config = {0};
        guitar_click_clock_t clock = {0};
        int16_t pcm[GUITAR_CLICK_SAMPLES];
        for (;;) {
            (void)xQueueReceive(s_mailbox, &config, pdMS_TO_TICKS(5));
            if (atomic_load(&s_stop_requested)) {
                /* The shutdown coordinator owns the final suspend sequence. */
                atomic_store(&s_stopped, true);
                vTaskDelete(NULL);
                return;
            }
            bool accent;
            if (!guitar_click_poll(&clock, &config, esp_timer_get_time(), &accent)) continue;
            size_t count = guitar_click_pcm(accent, config.volume, pcm, GUITAR_CLICK_SAMPLES);
            /* Short pulses only: the BSP's auto_clear_after_cb zeros consumed
             * DMA buffers. No looped PCM, silent feed, or queued future beats. */
            err = bsp_audio_write(pcm, count * sizeof(pcm[0]));
            if (err != ESP_OK) break;
        }
    }
    atomic_store(&s_status, -1);
    ESP_LOGE(TAG, "Audio unavailable: %s", esp_err_to_name(err));
    (void)bsp_audio_sleep(); /* This worker has stopped all PCM I/O. */
    atomic_store(&s_stopped, true);
    vTaskDelete(NULL);
}

esp_err_t guitar_audio_start(void)
{
    if (s_mailbox) return ESP_ERR_INVALID_STATE;
    atomic_store(&s_stop_requested, false);
    atomic_store(&s_stopped, false);
    s_mailbox = xQueueCreate(1, sizeof(guitar_audio_clock_t));
    if (!s_mailbox || xTaskCreate(worker, "guitar_audio", 4096, NULL, 6, NULL) != pdPASS) {
        if (s_mailbox) vQueueDelete(s_mailbox);
        s_mailbox = NULL;
        atomic_store(&s_status, -1);
        atomic_store(&s_stopped, true);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
