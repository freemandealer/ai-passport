#pragma once
#include <stdbool.h>
#include <stdint.h>

#define GUITAR_IDLE_TIMEOUT_US UINT64_C(300000000)

typedef struct {
    uint64_t since_us;
    uint32_t activity;
    bool initialized, was_playing;
} guitar_idle_t;

/* Count-in and playback inhibit shutdown. A transition to idle starts a full
 * five-minute interval. Call from one owner using monotonic time. */
bool guitar_idle_update(guitar_idle_t *idle, bool playing, uint32_t activity, uint64_t now_us);
