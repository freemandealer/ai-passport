#include "pitch_detector.h"
#include <math.h>

/* YIN cumulative mean normalized difference, with integer inner loops for C3.
 * 512-sample comparison window, 16 kHz, 65..1060 Hz. Scratch is caller-owned.
 * See de Cheveigne & Kawahara (2002). This detects monophonic periodic sound. */
pitch_frame_t pitch_detect(pitch_workspace_t *w, const int16_t *pcm) {
    pitch_frame_t r = {0};
    int32_t sum = 0;
    unsigned clipped = 0;
    for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) {
        sum += pcm[i];
        clipped += pcm[i] > 32000 || pcm[i] < -32000;
    }
    int32_t mean = sum / PITCH_FRAME_SAMPLES;
    uint64_t energy = 0;
    for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) {
        /* Division bounds the difference-square sum below UINT32_MAX. */
        int32_t v = ((int32_t)pcm[i] - mean) / 32;
        w->centered[i] = v;
        energy += v * v;
    }
    float rms = sqrtf((float)energy / PITCH_FRAME_SAMPLES) * 32.0f;
    r.level = (unsigned)fminf(100.0f, rms / 60.0f);
    r.clipped = clipped > PITCH_FRAME_SAMPLES / 50;
    if (rms < 180.0f || r.clipped) return r;
    float running = 0;
    w->difference[0] = 1.0f;
    for (int lag = 1; lag <= 250; ++lag) {
        uint32_t d = 0;
        for (int j = 0; j < 512; ++j) {
            int32_t delta = w->centered[j] - w->centered[j + lag];
            d += delta * delta;
        }
        running += (float)d;
        w->difference[lag] = running > 0 ? d * (float)lag / running : 1.0f;
    }
    int lag = 0;
    for (int i = 14; i < 249; ++i) {
        if (w->difference[i] < 0.15f) {
            while (i < 249 && w->difference[i + 1] < w->difference[i]) ++i;
            lag = i;
            break;
        }
    }
    if (!lag) return r; /* Noise/inharmonic input: never invent a note. */
    float a = w->difference[lag - 1], b = w->difference[lag], c = w->difference[lag + 1];
    float denominator = a - 2.0f * b + c;
    float offset = fabsf(denominator) > 1e-8f ? 0.5f * (a - c) / denominator : 0;
    if (offset < -0.5f || offset > 0.5f) offset = 0;
    r.hz = PITCH_SAMPLE_RATE / (lag + offset);
    r.midi = 69.0f + 12.0f * log2f(r.hz / 440.0f);
    r.clarity = 1.0f - b;
    r.voiced = r.hz >= 65.0f && r.hz <= 1060.0f;
    return r;
}

const char *pitch_note_name(int midi) {
    static const char *const names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return midi >= 0 ? names[midi % 12] : "--";
}
