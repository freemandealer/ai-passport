/* Execute the production capture loop and callback, with only hardware/RTOS/UI
 * boundaries replaced. The DSP and input reducer remain real. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../main/main.c"

static key_input_t mailbox;
static bool locked, sleeping, active, callback_context;
static unsigned reads, starts, stops, loops, stage, rounds;
static bool fail_read;
static int64_t clock_us;
static key_phase_t rendered;
static jmp_buf finished;
static void (*scenario)(void);

static void send_key(bsp_btn_t button, bsp_btn_ev_t event) {
    callback_context = true;
    on_button(button, event, NULL);
    callback_context = false;
}
QueueHandle_t xQueueCreate(unsigned count, unsigned size) {
    assert(count==1 && size==sizeof(mailbox)); return &mailbox;
}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *value) {
    assert(q==&mailbox); mailbox=*(const key_input_t *)value; return pdTRUE;
}
BaseType_t xQueuePeek(QueueHandle_t q,void *value,TickType_t timeout) {
    assert(q==&mailbox && !timeout); *(key_input_t *)value=mailbox; return pdTRUE;
}
const char *esp_err_to_name(esp_err_t e) { (void)e; return "test"; }
void test_log(const char *tag,const char *format,...) { (void)tag; (void)format; assert(!callback_context); }
int64_t esp_timer_get_time(void) { return clock_us; }
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 4096; }
esp_err_t bsp_audio_init(void) { return ESP_OK; }
esp_err_t bsp_audio_wake(void) { sleeping=false; return ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels) {
    assert(!callback_context && hz==16000 && bits==16 && channels==1);
    if (sleeping) return ESP_ERR_INVALID_STATE;
    active=true; ++starts; return ESP_OK;
}
void bsp_audio_set_volume(uint8_t value) { assert(!value); }
esp_err_t bsp_audio_sleep(void) { assert(!callback_context); sleeping=true; active=false; ++stops; return ESP_OK; }
esp_err_t bsp_audio_read(void *data,size_t bytes) {
    assert(!callback_context && active && !sleeping && bytes==512);
    ++reads; clock_us+=16000; memset(data,0,bytes);
    if (fail_read) { fail_read=false; return ESP_FAIL; }
    return ESP_OK;
}
esp_err_t bsp_battery_init(void) { return ESP_OK; }
int bsp_battery_soc(void) { assert(!active); return 80; }
bool bsp_lvgl_lock(int timeout) { (void)timeout; assert(!locked && !callback_context); return locked=true; }
void bsp_lvgl_unlock(void) { assert(locked); locked=false; }
bool key_ui_create(void) { return true; }
void key_ui_render(const key_session_t *s,int battery,uint32_t now) {
    (void)battery; (void)now; assert(locked && !callback_context); rendered=s->phase;
}
esp_err_t bsp_display_init(void) { return ESP_OK; }
void *bsp_lvgl_init(void) { return &mailbox; }
void bsp_display_backlight(uint8_t value) { (void)value; }
esp_err_t bsp_button_init(void (*cb)(bsp_btn_t,bsp_btn_ev_t,void *),void *user) { (void)cb; (void)user; return ESP_OK; }
BaseType_t xTaskCreate(TaskFunction_t f,const char *name,unsigned size,void *arg,unsigned priority,TaskHandle_t *task) {
    (void)f; (void)name; (void)size; (void)arg; (void)priority; (void)task; return pdPASS;
}
size_t esp_get_free_heap_size(void) { return 100000; }
size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return 50000; }
void vTaskDelay(TickType_t ticks) {
    assert(!locked && !callback_context && ++loops<10000);
    clock_us+=ticks*1000; scenario();
}
static void repeat_capture(void) {
    if (stage==0 && session.frames>=26) {
        send_key(BSP_BTN_OK,BSP_BTN_RELEASE); stage=1;
    } else if (stage==1 && rendered==KEY_QUIET) {
        assert(!active && session.frames>=26);
        send_key(BSP_BTN_DOWN,BSP_BTN_PRESS); stage=2;
    } else if (stage==2 && rendered==KEY_READY) {
        assert(session.frames==0 && session.first==-1 && !active);
        if (++rounds==3) longjmp(finished,1);
        send_key(BSP_BTN_OK,BSP_BTN_PRESS); stage=0;
    }
}
static void cancel_held(void) {
    if (stage==0 && session.frames>=3) {
        send_key(BSP_BTN_DOWN,BSP_BTN_PRESS); // DOWN before OK's release.
        send_key(BSP_BTN_OK,BSP_BTN_RELEASE); stage=1;
    } else if (stage==1 && rendered==KEY_READY) {
        assert(!active && !session.frames && !mailbox.capture);
        longjmp(finished,1);
    }
}
static void retry_after_error(void) {
    if (stage==0 && rendered==KEY_ERROR) {
        send_key(BSP_BTN_DOWN,BSP_BTN_PRESS); stage=1;
    } else if (stage==1 && rendered==KEY_READY) {
        assert(!active && session.frames==0);
        send_key(BSP_BTN_OK,BSP_BTN_RELEASE);
        send_key(BSP_BTN_OK,BSP_BTN_PRESS); stage=2;
    } else if (stage==2 && session.frames>=3) {
        assert(active && starts==2); longjmp(finished,1);
    }
}
static void run(void (*test)(void),bool read_fault) {
    memset(&callback_input,0,sizeof(callback_input));
    memset(&mailbox,0,sizeof(mailbox)); memset(&session,0,sizeof(session));
    sleeping=active=locked=callback_context=false;
    reads=starts=stops=loops=stage=rounds=0;
    clock_us=0; rendered=KEY_READY; fail_read=read_fault;
    input_queue=&mailbox; scenario=test;
    send_key(BSP_BTN_OK,BSP_BTN_PRESS);
    if (!setjmp(finished)) capture_task(NULL);
    assert(!locked);
}
int main(void) {
    run(repeat_capture,false); assert(starts==3 && stops==3);
    run(cancel_held,false); assert(starts==1 && stops==1);
    run(retry_after_error,true);
    puts("Capture worker: PASS (record/release/clear/re-record, held cancel, I/O error retry, LVGL lock)");
}
