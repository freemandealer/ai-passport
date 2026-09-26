#include "frog_model.h"

void frog_model_init(frog_model_t *m) {
    *m = (frog_model_t){ .phase = FROG_READY, .score = -1, .best = -1 };
}

static void finish(frog_model_t *m, uint32_t now) {
    m->phase_ms = now;
    m->level = 0;
    if (now - m->started_ms < FROG_MIN_MS || m->samples < 3200) {
        m->phase = FROG_TOO_SHORT;
    } else if (m->audible_samples < 640) {
        m->phase = FROG_TOO_QUIET;
    } else {
        m->phase = FROG_THINKING;
    }
}

void frog_model_input(frog_model_t *m, bool held, uint32_t now) {
    bool pressed = held && !m->held;
    bool released = !held && m->held;
    m->held = held;
    if (m->phase == FROG_INPUT_ERROR) return;
    if (released && m->phase == FROG_LISTENING) finish(m, now);
    if (pressed && m->phase != FROG_LISTENING &&
        m->phase != FROG_THINKING && m->phase != FROG_JUMPING) {
        m->phase = FROG_LISTENING;
        m->started_ms = now;
        m->samples = m->audible_samples = m->level = 0;
        m->score = -1;
    }
}

void frog_model_pcm(frog_model_t *m, const int16_t *pcm, size_t count) {
    if (m->phase != FROG_LISTENING || !pcm || !count) return;
    /* Peak-to-peak rejects DC offset; this is a simple silence guard, not ASR
     * or a calibrated speech detector. Scores never depend on this level. */
    int lo = 32767, hi = -32768;
    for (size_t i = 0; i < count; ++i) {
        if (pcm[i] < lo) lo = pcm[i];
        if (pcm[i] > hi) hi = pcm[i];
    }
    unsigned span = (unsigned)(hi - lo);
    unsigned level = span / 100;
    if (level > 100) level = 100;
    m->level = level > m->level ? level : (3 * m->level + level) / 4;
    m->samples += (uint32_t)count;
    if (span >= 300) m->audible_samples += (uint32_t)count;
}

int frog_score(uint32_t random) {
    return (int)(((uint64_t)random * 101) >> 32);
}

void frog_model_tick(frog_model_t *m, uint32_t now, uint32_t random) {
    if (m->phase == FROG_LISTENING && now - m->started_ms >= FROG_MAX_MS) {
        finish(m, now);
    } else if (m->phase == FROG_THINKING && now - m->phase_ms >= FROG_THINK_MS) {
        m->score = frog_score(random);
        if (m->score > m->best) m->best = m->score;
        m->phase = FROG_JUMPING;
        m->phase_ms = now;
    } else if (m->phase == FROG_JUMPING && now - m->phase_ms >= FROG_JUMP_MS) {
        m->phase = FROG_RESULT;
    }
}

int frog_jump_height(int score) {
    if (score < 0) score = 0;
    if (score > 100) score = 100;
    return score * FROG_MAX_HEIGHT / 100;
}

int frog_jump_offset(const frog_model_t *m, uint32_t now) {
    if (m->phase != FROG_JUMPING) return 0;
    uint32_t t = now - m->phase_ms;
    if (t >= FROG_JUMP_MS) return 0;
    return (int)((uint64_t)4 * frog_jump_height(m->score) * t *
                 (FROG_JUMP_MS - t) / (FROG_JUMP_MS * FROG_JUMP_MS));
}

void frog_model_error(frog_model_t *m) {
    m->phase = FROG_AUDIO_ERROR;
    m->score = -1;
    m->level = 0;
}

void frog_model_replay(frog_model_t *m, uint32_t now) {
    if (m->phase != FROG_RESULT) return;
    m->phase = FROG_JUMPING;
    m->phase_ms = now;
}

void frog_model_reset(frog_model_t *m) {
    if (m->held || m->phase == FROG_LISTENING || m->phase == FROG_INPUT_ERROR) return;
    int best = m->best;
    frog_model_init(m);
    m->best = best;
}
