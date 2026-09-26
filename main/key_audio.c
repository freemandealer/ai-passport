#include "key_audio.h"
#include "bsp_audio.h"
#include "pitch_detector.h"

esp_err_t key_audio_start(void) {
    esp_err_t error = bsp_audio_init();
    /* init is idempotent, not a wake operation. set_format rejects sleep. */
    if (error == ESP_OK) error = bsp_audio_wake();
    if (error == ESP_OK) error = bsp_audio_set_format(PITCH_SAMPLE_RATE, 16, 1);
    if (error == ESP_OK) bsp_audio_set_volume(0);
    return error;
}

esp_err_t key_audio_stop(void) {
    return bsp_audio_sleep();
}
