#pragma once
#include "esp_err.h"

/* Single capture-worker owner; no PCM read may overlap start/stop. */
esp_err_t key_audio_start(void);
esp_err_t key_audio_stop(void);
