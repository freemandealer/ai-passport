#pragma once
#include "key_analyzer.h"
#include "lvgl.h"

/* All calls require the LVGL task or BSP LVGL lock. Single lifetime screen. */
bool key_ui_create(void);
bool key_ui_check_fonts(void);
void key_ui_render(const key_session_t *s, int battery, uint32_t now_ms);
