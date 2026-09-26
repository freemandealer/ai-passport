#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PITCH_SAMPLE_RATE 16000
#define PITCH_FRAME_SAMPLES 2048
#define PITCH_FRAME_MS 128
#define PITCH_MIDI_LOW 36
#define PITCH_MIDI_COUNT 49
#define PITCH_MAX_FRAMES 234 /* Just under 30 seconds; bounded counters. */

typedef struct {
    float hz, midi, clarity;
    unsigned level;
    bool voiced, clipped;
} pitch_frame_t;

typedef struct {
    int16_t centered[PITCH_FRAME_SAMPLES];
    float difference[251];
} pitch_workspace_t;

pitch_frame_t pitch_detect(pitch_workspace_t *work, const int16_t *pcm);
const char *pitch_note_name(int midi);
