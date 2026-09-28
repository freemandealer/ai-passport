#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../main/guitar_power.c"

static jmp_buf done;
static uint64_t now;
static bool audio_stop, battery_stop, never_stop, lock_failure, sleep_returns;
static int service_error, peripheral_error;
static char calls[32];
static unsigned count;
static void call(char c) { assert(count + 1 < sizeof(calls)); calls[count++] = c; calls[count] = 0; }

const char *esp_err_to_name(esp_err_t err) { (void)err; return "test"; }
esp_err_t guitar_service_stop(void) { call('H'); return service_error; }
void guitar_audio_request_stop(void) { call('A'); audio_stop = true; }
void guitar_battery_request_stop(void) { call('B'); battery_stop = true; }
bool guitar_audio_stopped(void) { return audio_stop && !never_stop && now >= 60000; }
bool guitar_battery_stopped(void) { return battery_stop && !never_stop && now >= 100000; }
int64_t esp_timer_get_time(void) { return now; }
void vTaskDelay(TickType_t ticks) { assert(ticks == 20); now += ticks * 1000; }
esp_err_t esp_sleep_disable_wakeup_source(int source)
{ assert(source == ESP_SLEEP_WAKEUP_ALL); call('W'); return ESP_OK; }
esp_err_t bsp_battery_sleep(void)
{ assert(guitar_audio_stopped() && guitar_battery_stopped()); call('b'); return peripheral_error; }
esp_err_t bsp_audio_sleep(void) { call('a'); return peripheral_error; }
esp_err_t bsp_audio_prepare_deep_sleep(void) { call('I'); return peripheral_error; }
esp_err_t bsp_i2c_prepare_deep_sleep(void) { call('i'); return peripheral_error; }
bool bsp_lvgl_lock(int timeout) { assert(timeout == 1000); call('L'); return !lock_failure; }
esp_err_t bsp_display_prepare_deep_sleep(void) { call('D'); return peripheral_error; }
void esp_deep_sleep_start(void) { call('S'); if (!sleep_returns) longjmp(done, 1); }
void esp_restart(void) { call('R'); longjmp(done, 2); }

static void reset(void)
{
    now = count = 0; calls[0] = 0;
    audio_stop = battery_stop = never_stop = lock_failure = sleep_returns = false;
    service_error = peripheral_error = ESP_OK;
}
static int run(void)
{
    int result = setjmp(done);
    if (!result) guitar_power_off();
    return result;
}
int main(void)
{
    assert(guitar_power_activity() && !guitar_power_closing());
    assert(!guitar_power_idle_due(false, 0));
    assert(!guitar_power_idle_due(false, GUITAR_IDLE_TIMEOUT_US - 1));
    assert(guitar_power_activity());
    assert(!guitar_power_idle_due(false, GUITAR_IDLE_TIMEOUT_US));
    assert(guitar_power_idle_due(false, 2*GUITAR_IDLE_TIMEOUT_US));
    assert(guitar_power_closing() && !guitar_power_activity());
    assert(!guitar_power_idle_due(false, 3*GUITAR_IDLE_TIMEOUT_US));
    reset(); assert(run() == 1 && !strcmp(calls, "HABWbaIiLDS") && now == 100000);
    reset(); service_error = ESP_FAIL;
    assert(run() == 2 && !strcmp(calls, "HR"));
    reset(); never_stop = true;
    assert(run() == 2 && !strcmp(calls, "HABR") && now == 8000000);
    reset(); peripheral_error = ESP_FAIL;
    assert(run() == 1 && !strcmp(calls, "HABWbaIiLDS"));
    reset(); lock_failure = true;
    assert(run() == 2 && !strcmp(calls, "HABWbaIiLR"));
    reset(); sleep_returns = true;
    assert(run() == 2 && !strcmp(calls, "HABWbaIiLDSR"));
    puts("Power: PASS (activity cutoff, HTTP drain, cooperative stop, peripheral order, no wake timer, failure recovery)");
    return 0;
}
