#pragma once
#include "guitar_metronome.h"
#include "esp_err.h"

/* One lifetime worker owns every codec call. Call start once at boot. */
esp_err_t guitar_audio_start(void);
/* Non-blocking mailbox; caller serializes publishers with the model mutex. */
void guitar_audio_publish(const guitar_audio_clock_t *clock);
/* 0 initializing, 1 ready, -1 failed; a failure leaves visual playback usable. */
int guitar_audio_status(void);
