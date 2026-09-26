/* Reuse the real BSP's existing codec/I2S fault-injection fixtures. */
#define main bsp_recovery_test_entry
#include "test_bsp_audio_recovery.c"
#undef main
#include "key_audio.h"

int main(void) {
    fresh();
    for (unsigned capture = 0; capture < 30; ++capture) {
        esp_err_t error = key_audio_start();
        if (error != ESP_OK) fprintf(stderr, "capture %u start failed: %d\n", capture + 1, error);
        assert(error == ESP_OK);
        assert_active();
        assert(key_audio_stop() == ESP_OK);
        assert_rollback();
        assert(s_sleeping);
    }
    fault(0x0d, 0xfa, 1);
    assert(key_audio_start() != ESP_OK);
    assert_rollback();
    assert(key_audio_start() == ESP_OK); // Transient wake failure is retryable.
    assert_active();
    fault(0x0d, 0xfc, 2);
    assert(key_audio_stop() != ESP_OK);
    assert_rollback();
    assert(key_audio_start() == ESP_OK); // Failed suspend cannot strand retries.
    assert_active();
    assert(key_audio_stop() == ESP_OK);
    audio_cleanup();
    assert(ctrl_creations == ctrl_deletions);
    puts("Application audio: PASS (30 captures, wake/suspend faults and retry)");
}
