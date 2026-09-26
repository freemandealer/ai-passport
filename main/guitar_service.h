#pragma once
#include "guitar_score.h"
#include "guitar_ui.h"
#include "esp_err.h"

typedef esp_err_t (*guitar_apply_fn)(const guitar_library_t *candidate, const char *text, size_t length);

/* Startup and HTTP/NVS operations run in worker context, without the LVGL lock. */
void guitar_service_load(guitar_library_t *library, guitar_network_info_t *info);
esp_err_t guitar_service_start(guitar_apply_fn apply, const guitar_network_info_t *info);
/* Called by apply while the model is paused and locked. Updates GET text only
 * after nvs_commit succeeds; no erase or fallback overwrite on failure. */
esp_err_t guitar_service_save(const char *text, size_t length);
