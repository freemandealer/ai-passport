#include "guitar_metronome.h"

bool guitar_click_poll(guitar_click_clock_t *clock, const guitar_audio_clock_t *c, uint64_t now, bool *accent)
{
    if (!c->active || !c->volume || c->bpm < 30 || c->bpm > 120 ||
        now < c->start_us || now >= c->end_us) {
        clock->synced = false;
        return false;
    }
    uint64_t elapsed = now - c->start_us;
    if (elapsed > UINT64_C(86400000000)) return false;
    uint64_t step = elapsed * c->bpm / UINT64_C(30000000);
    if (clock->synced && clock->start_us == c->start_us && clock->step == step) return false;
    clock->synced = true; clock->start_us = c->start_us; clock->step = step;
    uint64_t boundary = (step * UINT64_C(30000000) + c->bpm - 1) / c->bpm;
    if (elapsed - boundary > 50000) return false;
    *accent = step % 2 == 0;
    return true;
}

size_t guitar_click_pcm(bool accent, uint8_t volume, int16_t *pcm, size_t capacity)
{
    static const int16_t sine[] = {
        0,6393,12539,18204,23170,27245,30273,32137,
        32767,32137,30273,27245,23170,18204,12539,6393,
        0,-6393,-12539,-18204,-23170,-27245,-30273,-32137,
        -32767,-32137,-30273,-27245,-23170,-18204,-12539,-6393
    };
    size_t n = accent ? GUITAR_CLICK_SAMPLES : 320;
    if (capacity < n) return 0;
    if (volume > 10) volume = 10;
    int amplitude = (accent ? 12000 : 4000) * volume * volume / 100;
    for (size_t i = 0; i < n; ++i) {
        int wave = accent ? (4 * sine[(i * 2) % 32] + sine[(i * 6) % 32]) / 5 : sine[(i * 4) % 32];
        /* Short attack and a smooth squared decay avoid discontinuities. */
        int envelope = i < 16 ? (int)i * 1024 / 16 : (int)(n - 1 - i) * 1024 / (n - 17);
        int sample = wave * amplitude / 32768;
        sample = sample * envelope / 1024;
        sample = sample * envelope / 1024;
        pcm[i] = (int16_t)sample;
    }
    return n;
}
