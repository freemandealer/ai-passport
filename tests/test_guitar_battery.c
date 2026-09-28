#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../main/guitar_battery.c"

static jmp_buf done;
static unsigned inits, reads, updates, delays;
static unsigned waited;
static bool missing, stop_early, fail_task;
static void (*entry)(void *);
static const int readings[] = {67, -1, 66, 0, 100, 255, 99};
static const int expected[] = {-1, -1, 67, -1, 66, 0, 100, -1, 99};
static const unsigned waits[] = {5000,5000,15000,5000,15000,15000,15000,5000,15000};

esp_err_t bsp_battery_init(void)
{
    ++inits;
    return missing || inits < 3 ? ESP_FAIL : ESP_OK;
}
int bsp_battery_soc(void)
{
    assert(!missing && reads < sizeof(readings) / sizeof(readings[0]));
    return readings[reads++];
}
int xQueueOverwrite(QueueHandle_t queue, const void *item)
{
    assert(queue == (void *)1 && updates < sizeof(expected) / sizeof(expected[0]));
    assert(*(const int *)item == (missing ? -1 : expected[updates]));
    ++updates;
    return pdTRUE;
}
void vTaskDelay(TickType_t ticks)
{
    assert(updates == delays + 1);
    assert(ticks == 100);
    waited += ticks;
    if (stop_early) { guitar_battery_request_stop(); return; }
    if (waited < (missing ? 5000 : waits[delays])) return;
    assert(waited == (missing ? 5000 : waits[delays]));
    waited = 0;
    if (++delays == sizeof(expected) / sizeof(expected[0])) longjmp(done, 1);
}
int xTaskCreate(void (*fn)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *task)
{ (void)name; (void)task; assert(stack >= 3072 && arg == (void *)1 && priority == 2); entry = fn; return !fail_task; }
void vTaskDelete(TaskHandle_t task)
{ (void)task; assert(guitar_battery_stopped()); longjmp(done, 2); }
static void reset(void)
{
    inits = reads = updates = delays = waited = 0;
    missing = stop_early = fail_task = false;
    s_started = false;
}
int main(void)
{
    assert(guitar_battery_start((void *)1) == ESP_OK);
    assert(!guitar_battery_stopped());
    if (!setjmp(done)) entry((void *)1);
    assert(inits == 3 && reads == 7 && updates == 9);
    reset(); missing = true;
    assert(guitar_battery_start((void *)1) == ESP_OK);
    if (!setjmp(done)) entry((void *)1);
    assert(inits == 9 && reads == 0 && updates == 9);
    reset(); stop_early = true;
    assert(guitar_battery_start((void *)1) == ESP_OK);
    if (!setjmp(done)) entry((void *)1);
    assert(guitar_battery_stopped() && updates == 1 && inits == 1 && waited == 100);
    reset(); fail_task = true;
    assert(guitar_battery_start((void *)1) == ESP_ERR_NO_MEM && guitar_battery_stopped());
    puts("Battery worker: PASS (initialization retry, read recovery, 0/100%, invalid SOC, missing gauge, bounded polling)");
    return 0;
}
