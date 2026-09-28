#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Non-blocking; false means terminal shutdown already claimed the device. */
bool guitar_power_activity(void);
/* Main model owner only. Atomically claims shutdown if inactivity expires;
 * an activity event racing this claim wins and restarts the interval. */
bool guitar_power_idle_due(bool playing, uint64_t now_us);
bool guitar_power_closing(void);
/* Terminal; invoke outside model/LVGL locks after claiming shutdown. */
void guitar_power_off(void);
