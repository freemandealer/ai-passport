#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GUITAR_AUDIO_RATE 16000
#define GUITAR_CLICK_SAMPLES 560

typedef struct {
    uint64_t start_us, end_us;
    uint16_t bpm;
    uint8_t volume;
    bool active;
} guitar_audio_clock_t;
typedef struct { uint64_t start_us, step; bool synced; } guitar_click_clock_t;

/* Returns at most the current click, never a backlog after a stalled worker. */
bool guitar_click_poll(guitar_click_clock_t *clock, const guitar_audio_clock_t *config,
                       uint64_t now_us, bool *accent);
/* 1 kHz + harmonic, 35 ms on quarter notes; softer 2 kHz, 20 ms offbeats. */
size_t guitar_click_pcm(bool accent, uint8_t volume, int16_t *pcm, size_t capacity);
