#pragma once
#include "frog_model.h"
#include "lvgl.h"

/* Caller owns the LVGL lock. One permanent screen, no background UI producers. */
bool frog_ui_create(void);
void frog_ui_render(const frog_model_t *model, uint32_t now, int battery);
bool frog_ui_check_fonts(void);
