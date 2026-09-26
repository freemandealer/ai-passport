#pragma once
#include "guitar_player.h"
#include "lvgl.h"

typedef struct {
    char ssid[24];
    bool ready, storage_ok, audio_failed;
} guitar_network_info_t;

/* Caller holds the BSP LVGL lock. Screens live for the application lifetime. */
bool guitar_ui_create(void);
void guitar_ui_render(const guitar_player_t *player, const guitar_library_t *library,
                      int battery, const guitar_network_info_t *network);
bool guitar_ui_check_fonts(void);
LV_FONT_DECLARE(guitar_font_16);
LV_FONT_DECLARE(guitar_font_24);
