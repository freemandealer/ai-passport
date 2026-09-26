#include "guitar_metronome.h"
#include <assert.h>
#include <stdio.h>

static uint64_t energy(const int16_t *p, size_t n)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < n; ++i) { int32_t s = p[i]; sum += s * s; }
    return sum;
}

int main(void)
{
    int16_t strong[GUITAR_CLICK_SAMPLES], weak[GUITAR_CLICK_SAMPLES];
    uint64_t previous = 0;
    for (unsigned level = 0; level <= 10; ++level) {
        size_t ns = guitar_click_pcm(true, level, strong, GUITAR_CLICK_SAMPLES);
        size_t nw = guitar_click_pcm(false, level, weak, GUITAR_CLICK_SAMPLES);
        assert(ns == 560 && nw == 320 && strong[0] == 0 && strong[ns - 1] == 0 && weak[nw - 1] == 0);
        uint64_t es = energy(strong, ns), ew = energy(weak, nw);
        if (!level) assert(!es && !ew);
        else { assert(es > 4 * ew && ew > 0 && es > previous); }
        previous = es;
        for (size_t i = 0; i < ns; ++i) assert(strong[i] <= 12000 && strong[i] >= -12000);
    }
    assert(guitar_click_pcm(true, 5, strong, 559) == 0);
    /* Different periods: strong has the 1 kHz fundamental; weak has 2 kHz. */
    assert(strong[4] > 0 && strong[12] < 0);
    assert(weak[2] > 0 && weak[6] < 0);
    for (unsigned bpm = 30; bpm <= 120; ++bpm) {
        guitar_click_clock_t clock = {0};
        guitar_audio_clock_t config = {.start_us = 1234567, .end_us = UINT64_C(86400000000), .bpm = bpm, .volume = 5, .active = true};
        bool accent;
        assert(!guitar_click_poll(&clock, &config, config.start_us - 1, &accent));
        for (unsigned beat = 0; beat < 80; ++beat) {
            uint64_t due = config.start_us + (beat * UINT64_C(30000000) + bpm - 1) / bpm;
            if (beat) assert(!guitar_click_poll(&clock, &config, due - 1, &accent));
            assert(guitar_click_poll(&clock, &config, due, &accent));
            assert(accent == (beat % 2 == 0));
            assert(!guitar_click_poll(&clock, &config, due, &accent));
            assert(!guitar_click_poll(&clock, &config, due + 5000, &accent));
        }
        config.active = false;
        assert(!guitar_click_poll(&clock, &config, config.start_us, &accent));
        config.active = true; config.volume = 0;
        assert(!guitar_click_poll(&clock, &config, config.start_us, &accent));
        config.volume = 5; config.start_us += UINT64_C(200000000);
        assert(guitar_click_poll(&clock, &config, config.start_us, &accent) && accent);
        uint64_t late = config.start_us + (3 * UINT64_C(30000000) + bpm - 1) / bpm + 80000;
        assert(!guitar_click_poll(&clock, &config, late, &accent));
        assert(!guitar_click_poll(&clock, &config, late + 1, &accent));
        uint64_t next = config.start_us + (4 * UINT64_C(30000000) + bpm - 1) / bpm;
        assert(guitar_click_poll(&clock, &config, next, &accent) && accent);
        config.end_us = next;
        assert(!guitar_click_poll(&clock, &config, next, &accent));
    }
    puts("Metronome: PASS (91 tempos, eighth-note deadlines, no catch-up bursts, mute, volume, distinct accent PCM)");
    return 0;
}
