#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../main/guitar_audio.c"

static jmp_buf done;
static void (*entry)(void *);
static guitar_audio_clock_t pending;
static bool available, fail_task;
static int init_error, format_error, write_error;
static uint64_t now;
static unsigned writes, sleeps, formats, deletes, levels;
static uint64_t timestamps[10];
static size_t sizes[10];

const char *esp_err_to_name(esp_err_t e) { (void)e; return "test"; }
QueueHandle_t xQueueCreate(unsigned n, unsigned size) { assert(n == 1 && size == sizeof(pending)); return (void *)1; }
int xTaskCreate(void (*task)(void *), const char *name, unsigned stack, void *arg, unsigned priority, TaskHandle_t *handle)
{ (void)name; (void)arg; (void)handle; assert(stack >= 4096 && priority == 6); entry = task; return !fail_task; }
void vQueueDelete(QueueHandle_t queue) { (void)queue; ++deletes; }
void vTaskDelete(TaskHandle_t task) { (void)task; longjmp(done, 2); }
int xQueueOverwrite(QueueHandle_t queue, const void *item)
{ (void)queue; memcpy(&pending, item, sizeof(pending)); available = true; return pdTRUE; }
int xQueueReceive(QueueHandle_t queue, void *item, TickType_t wait)
{
    (void)queue; assert(wait == 5);
    now += 5000;
    if (now == 800000) { guitar_audio_clock_t c = pending; c.active = false; guitar_audio_publish(&c); }
    if (now == 900000) { guitar_audio_clock_t c = pending; c.active = true; c.start_us = now; c.end_us = 1700000; guitar_audio_publish(&c); }
    if (now > 1800000) longjmp(done, 1);
    if (!available) return pdFALSE;
    memcpy(item, &pending, sizeof(pending)); available = false; return pdTRUE;
}
int64_t esp_timer_get_time(void) { return now; }
esp_err_t bsp_audio_init(void) { return init_error; }
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch)
{ assert(hz == 16000 && bits == 16 && ch == 1); ++formats; return format_error; }
void bsp_audio_set_volume(uint8_t v) { assert(v == 75); ++levels; }
esp_err_t bsp_audio_sleep(void) { ++sleeps; return ESP_OK; }
esp_err_t bsp_audio_write(const void *pcm, size_t bytes)
{
    assert(pcm && writes < 10);
    timestamps[writes] = now; sizes[writes++] = bytes;
    return write_error;
}
static void reset(void)
{
    s_mailbox = NULL; s_published = (guitar_audio_clock_t){0};
    atomic_store(&s_status, 0);
    available = fail_task = false; now = writes = sleeps = formats = deletes = levels = 0;
    init_error = format_error = write_error = ESP_OK;
}
static int run(void)
{
    assert(guitar_audio_start() == ESP_OK);
    guitar_audio_clock_t config = {.active = true, .bpm = 80, .volume = 5, .start_us = 100000, .end_us = 5000000};
    guitar_audio_publish(&config);
    int result = setjmp(done);
    if (!result) entry(NULL);
    return result;
}
int main(void)
{
    reset(); assert(run() == 1);
    assert(writes == 5 && formats == 1 && levels == 1 && !sleeps && guitar_audio_status() == 1);
    const uint64_t times[] = {100000,475000,900000,1275000,1650000};
    const size_t lengths[] = {1120,640,1120,640,1120};
    for (unsigned i = 0; i < writes; ++i) assert(timestamps[i] == times[i] && sizes[i] == lengths[i]);
    reset(); init_error = ESP_FAIL; assert(run() == 2);
    assert(guitar_audio_status() == -1 && !writes && !formats && sleeps == 1);
    reset(); format_error = ESP_FAIL; assert(run() == 2);
    assert(guitar_audio_status() == -1 && !writes && formats == 1 && sleeps == 1);
    reset(); write_error = ESP_FAIL; assert(run() == 2);
    assert(guitar_audio_status() == -1 && writes == 1 && sleeps == 1);
    reset(); fail_task = true;
    assert(guitar_audio_start() == ESP_ERR_NO_MEM && !s_mailbox && deletes == 1 && guitar_audio_status() == -1);
    puts("Audio worker: PASS (format/codec/task faults, PCM ownership, pause/resume, song end, bounded mailbox)");
    return 0;
}
