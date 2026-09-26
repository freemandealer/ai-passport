#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../main/guitar_battery.c"

static jmp_buf done;
static unsigned inits, reads, updates, delays;
static bool missing;
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
    assert(ticks == (missing ? 5000 : waits[delays]));
    if (++delays == sizeof(expected) / sizeof(expected[0])) longjmp(done, 1);
}
int main(void)
{
    if (!setjmp(done)) guitar_battery_worker((void *)1);
    assert(inits == 3 && reads == 7 && updates == 9);
    inits = reads = updates = delays = 0; missing = true;
    if (!setjmp(done)) guitar_battery_worker((void *)1);
    assert(inits == 9 && reads == 0 && updates == 9);
    puts("Battery worker: PASS (initialization retry, read recovery, 0/100%, invalid SOC, missing gauge, bounded polling)");
    return 0;
}
