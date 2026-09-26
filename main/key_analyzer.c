#include "key_analyzer.h"
#include "key_tables.h"
#include <math.h>
#include <string.h>

_Static_assert(KEY_FFT_SAMPLES == 2 * PITCH_FRAME_SAMPLES, "FFT window must span two hops");
_Static_assert(sizeof(key_hann) / sizeof(key_hann[0]) == KEY_FFT_SAMPLES, "Regenerate FFT tables");

/* Krumhansl-Kessler major/minor pitch-class profiles (C-based).
 * Correlation is a similarity score, never a calibrated probability. */
static const float profiles[2][12] = {
    {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f},
    {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f},
};

void key_rank(const float chroma[12], int *first, int *second, float *score, float *runner) {
    float mean = 0, variance = 0;
    *first = *second = -1;
    *score = *runner = -1;
    for (int i = 0; i < 12; ++i) mean += chroma[i] / 12.0f;
    for (int i = 0; i < 12; ++i) variance += (chroma[i] - mean) * (chroma[i] - mean);
    if (variance < 1e-8f) return;
    for (int mode = 0; mode < 2; ++mode) {
        float pm = 0;
        for (int i = 0; i < 12; ++i) pm += profiles[mode][i] / 12.0f;
        for (int tonic = 0; tonic < 12; ++tonic) {
            float covariance = 0, pv = 0;
            for (int i = 0; i < 12; ++i) {
                float p = profiles[mode][(i - tonic + 12) % 12] - pm;
                covariance += (chroma[i] - mean) * p;
                pv += p * p;
            }
            float r = covariance / sqrtf(variance * pv);
            int key = mode * 12 + tonic;
            if (r > *score) {
                *runner = *score; *second = *first;
                *score = r; *first = key;
            } else if (r > *runner) { *runner = r; *second = key; }
        }
    }
}

/* Q15 radix-2 FFT; every stage scales by 2 to keep 16-bit RAM bounded.
 * Hann window and twiddles live in flash. No per-butterfly software floats. */
static bool spectral_chroma(key_workspace_t *w, const int16_t *pcm, float out[12], bool ready) {
    const unsigned n = KEY_FFT_SAMPLES;
    if (!ready) {
        memcpy(w->previous, pcm, sizeof(w->previous));
        return false;
    }
    int32_t mean = 0;
    for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) mean += pcm[i] + w->previous[i];
    mean /= (int32_t)n;
    for (unsigned i = 0, j = 0; i < n; ++i) {
        int32_t sample = i < PITCH_FRAME_SAMPLES ? w->previous[i] : pcm[i - PITCH_FRAME_SAMPLES];
        int32_t centered = (sample - mean) / 2;
        w->real[j] = centered * key_hann[i] / 32768;
        w->imag[j] = 0;
        unsigned bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
    }
    memcpy(w->previous, pcm, sizeof(w->previous));
    for (unsigned length = 2; length <= n; length <<= 1) {
        unsigned half = length / 2, stride = n / length;
        for (unsigned base = 0; base < n; base += length) {
            for (unsigned j = 0; j < half; ++j) {
                unsigned a = base + j, b = a + half, t = j * stride;
                int32_t re = ((int32_t)key_twiddle[t][0] * w->real[b] - (int32_t)key_twiddle[t][1] * w->imag[b]) / 32768;
                int32_t im = ((int32_t)key_twiddle[t][0] * w->imag[b] + (int32_t)key_twiddle[t][1] * w->real[b]) / 32768;
                w->real[b] = (w->real[a] - re) / 2;
                w->imag[b] = (w->imag[a] - im) / 2;
                w->real[a] = (w->real[a] + re) / 2;
                w->imag[a] = (w->imag[a] + im) / 2;
            }
        }
    }
    uint32_t peak = 0;
    float total = 0;
    /* 256 ms windows, 128 ms hop: include C2 bass previously discarded. */
    for (unsigned i = 15; i < KEY_SPECTRUM_BINS; ++i) {
        int32_t re = w->real[i], im = w->imag[i];
        w->power[i] = re * re + im * im;
        if (w->power[i] > peak) peak = w->power[i];
        total += (float)w->power[i];
    }
    if (peak < 20 || peak < total * 0.045f) return false;
    /* Retain weak fundamentals as well as loud upper partials. */
    struct { float hz, midi, amplitude, weight; } peaks[64];
    unsigned count = 0;
    float tuning_histogram[20] = {0};
    for (unsigned i = 17; i < 537 && count < 64; ++i) {
        float a = w->power[i - 1], b = w->power[i], c = w->power[i + 1];
        if (b < peak * 0.015f || b <= a || b < c) continue;
        /* Parabolic log-power interpolation limits off-bin pitch bias. */
        a = logf(a + 1); b = logf(b + 1); c = logf(c + 1);
        float den = a - 2 * b + c;
        float delta = fabsf(den) > 1e-6f ? 0.5f * (a - c) / den : 0;
        if (delta < -0.5f || delta > 0.5f) delta = 0;
        float hz = (i + delta) * PITCH_SAMPLE_RATE / n;
        float midi = 69 + 12 * log2f(hz / 440);
        float amplitude = sqrtf((float)w->power[i] / peak);
        peaks[count].hz = hz;
        peaks[count].midi = midi;
        peaks[count].amplitude = amplitude;
        peaks[count].weight = amplitude;
        /* A spectral partial is not another independently played note.
         * Preserve some weight because a real chord tone can coincide with it. */
        for (unsigned lower = 0; lower < count; ++lower) {
            float ratio = hz / peaks[lower].hz;
            int harmonic = (int)lroundf(ratio);
            if (harmonic >= 2 && harmonic <= 6 &&
                fabsf(ratio - harmonic) < harmonic * 0.012f &&
                peaks[lower].amplitude >= amplitude * 0.12f) {
                peaks[count].weight *= 0.25f;
                break;
            }
        }
        float residual = midi - floorf(midi);
        unsigned bin = (unsigned)(residual * 20.0f + 0.5f) % 20;
        tuning_histogram[bin] += peaks[count].weight;
        ++count;
    }
    /* Estimate a common detuning before rounding to pitch classes. Previously
     * +/-38..50-cent peaks were discarded, including an entire detuned song. */
    unsigned tuning_bin = 0;
    float tuning_best = 0, tuning_total = 0;
    for (unsigned i = 0; i < 20; ++i) {
        tuning_total += tuning_histogram[i];
        float weight = tuning_histogram[i] + 0.5f *
            (tuning_histogram[(i + 19) % 20] + tuning_histogram[(i + 1) % 20]);
        if (weight > tuning_best) { tuning_best = weight; tuning_bin = i; }
    }
    float tuning = tuning_bin <= 10 ? tuning_bin * 0.05f : (tuning_bin - 20.0f) * 0.05f;
    if (tuning_best < tuning_total * 0.3f) tuning = 0;
    float weight_sum = 0;
    for (unsigned i = 0; i < count; ++i) {
        float midi = peaks[i].midi - tuning;
        int note = (int)lroundf(midi);
        float distance = fabsf(midi - note);
        /* Smooth weighting replaces a hard +/-38-cent dropout boundary. */
        float weight = peaks[i].weight * (1.0f - distance * distance * 3.0f);
        out[note % 12] += weight;
        weight_sum += weight;
    }
    if (weight_sum < 0.1f) return false;
    for (unsigned i = 0; i < 12; ++i) out[i] /= weight_sum;
    return true;
}

void key_session_clear(key_session_t *s, key_mode_t mode) {
    memset(s, 0, sizeof(*s));
    s->phase = KEY_READY; s->mode = mode;
    s->first = s->second = s->previous_note = -1;
}

void key_session_begin(key_session_t *s, key_mode_t mode) {
    key_session_clear(s, mode);
    s->phase = KEY_LISTENING;
}

void key_session_feed(key_session_t *s, key_workspace_t *w, const int16_t *pcm) {
    if (s->phase != KEY_LISTENING) return;
    pitch_frame_t pitch = pitch_detect(&w->pitch, pcm);
    ++s->frames; s->level = pitch.level; s->clipped += pitch.clipped;
    float frame[12] = {0};
    bool usable = false;
    if (s->mode == KEY_HUM) {
        int note = pitch.voiced ? (int)lroundf(pitch.midi) : -1;
        /* Two matching windows reject attacks and quick glissando crossings. */
        if (note >= 0 && note == s->previous_note) {
            frame[note % 12] = 1;
            usable = true;
        }
        s->previous_note = note;
    } else if (pitch.level >= 3 && !pitch.clipped) {
        usable = spectral_chroma(w, pcm, frame, s->spectral_ready);
        s->spectral_ready = true;
    } else {
        s->spectral_ready = false; // Never splice pre-silence audio into a new attack.
    }
    if (usable) {
        ++s->usable;
        for (unsigned i = 0; i < 12; ++i) s->chroma[i] += frame[i];
    }
    if (s->frames >= PITCH_MAX_FRAMES) {
        s->limit_reached = true;
        key_session_finish(s);
    }
}

void key_session_finish(key_session_t *s) {
    if (s->phase != KEY_LISTENING) return;
    s->level = 0;
    if (s->frames * PITCH_FRAME_MS < 3000) { s->phase = KEY_SHORT; return; }
    if (s->clipped * 5 > s->frames) { s->phase = KEY_CLIPPED; return; }
    if (s->usable < 12 || s->usable * 5 < s->frames) { s->phase = KEY_QUIET; return; }
    for (unsigned i = 0; i < 12; ++i) s->distinct += s->chroma[i] >= s->usable * 0.035f;
    if (s->distinct < 4) { s->phase = KEY_FEW_NOTES; return; }
    key_rank(s->chroma, &s->first, &s->second, &s->score, &s->runner_score);
    s->phase = s->first < 0 || s->score < 0.60f || s->score - s->runner_score < 0.08f
        ? KEY_AMBIGUOUS : KEY_RESULT;
}
