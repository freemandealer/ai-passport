#pragma once

/* Lifetime worker; arg is a one-slot queue of int percentages. Shared I2C is
 * initialized by app_main before starting device workers. No LVGL access. */
void guitar_battery_worker(void *arg);
