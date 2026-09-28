#include "guitar_idle.h"

bool guitar_idle_update(guitar_idle_t *s, bool playing, uint32_t activity, uint64_t now)
{
    if (!s->initialized || playing || s->was_playing || activity != s->activity || now < s->since_us)
        s->since_us = now;
    s->initialized = true;
    s->was_playing = playing;
    s->activity = activity;
    return !playing && now - s->since_us >= GUITAR_IDLE_TIMEOUT_US;
}
