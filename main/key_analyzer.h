#pragma once
#include "pitch_detector.h"

#define KEY_FFT_SAMPLES 4096
#define KEY_SPECTRUM_BINS 552 /* Through 2152 Hz at 16 kHz. */

typedef enum { KEY_HUM, KEY_MUSIC } key_mode_t;
typedef enum {
    KEY_READY, KEY_LISTENING, KEY_RESULT, KEY_AMBIGUOUS,
    KEY_SHORT, KEY_QUIET, KEY_FEW_NOTES, KEY_CLIPPED, KEY_ERROR,
} key_phase_t;
typedef struct {
    key_phase_t phase;
    key_mode_t mode;
    unsigned frames, usable, clipped, level, distinct;
    float chroma[12];
    int first, second; /* 0..11 major, 12..23 minor, C-based tonic. */
    float score, runner_score;
    bool limit_reached;
    bool spectral_ready;
    int previous_note;
} key_session_t;

typedef struct {
    pitch_workspace_t pitch;
    int16_t previous[PITCH_FRAME_SAMPLES];
    int16_t real[KEY_FFT_SAMPLES], imag[KEY_FFT_SAMPLES];
    uint32_t power[KEY_SPECTRUM_BINS];
} key_workspace_t;

void key_session_begin(key_session_t *s, key_mode_t mode);
void key_session_clear(key_session_t *s, key_mode_t mode);
void key_session_feed(key_session_t *s, key_workspace_t *work, const int16_t *pcm);
void key_session_finish(key_session_t *s);
/* Separated for deterministic profile/transposition tests. */
void key_rank(const float chroma[12], int *first, int *second, float *score, float *runner);
