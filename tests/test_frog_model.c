#include "frog_model.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void voice(frog_model_t *m) {
    int16_t pcm[320];
    for (unsigned i = 0; i < 320; ++i) pcm[i] = i % 2 ? 1000 : -1000;
    for (int i = 0; i < 20; ++i) frog_model_pcm(m, pcm, 320);
}

static void round_at(frog_model_t *m, uint32_t now, uint32_t random) {
    frog_model_input(m, true, now);
    assert(m->phase == FROG_LISTENING);
    voice(m);
    frog_model_input(m, false, now + 500);
    assert(m->phase == FROG_THINKING && m->score == -1);
    frog_model_tick(m, now + 1049, random);
    assert(m->phase == FROG_THINKING);
    frog_model_tick(m, now + 1050, random);
    assert(m->phase == FROG_JUMPING);
    for (uint32_t t = 0; t <= FROG_JUMP_MS; ++t) {
        int y = frog_jump_offset(m, now + 1050 + t);
        assert(y >= 0 && y <= FROG_MAX_HEIGHT);
    }
    assert(frog_jump_offset(m, now + 1050 + FROG_JUMP_MS / 2) == frog_jump_height(m->score));
    frog_model_tick(m, now + 2150, 0);
    assert(m->phase == FROG_RESULT);
}

int main(void) {
    frog_model_t m;
    frog_model_init(&m);
    assert(m.score == -1 && m.best == -1);
    round_at(&m, 0, 0);
    assert(m.score == 0);
    round_at(&m, 3000, UINT32_MAX);
    assert(m.score == 100 && m.best == 100);
    frog_model_replay(&m, 6000);
    assert(m.phase == FROG_JUMPING && m.score == 100);
    frog_model_reset(&m);
    assert(m.phase == FROG_READY && m.best == 100 && m.score == -1);
    // Wraparound and all 101 outcomes remain valid; heights are monotonic.
    round_at(&m, UINT32_MAX - 100, 0x80000000);
    assert(m.score == 50);
    bool seen[101] = {0};
    int last_height = -1;
    for (int s = 0; s <= 100; ++s) {
        seen[frog_score((uint32_t)(((uint64_t)s * UINT32_MAX) / 100))] = true;
        int height = frog_jump_height(s);
        assert(height >= last_height);
        last_height = height;
    }
    for (int i = 0; i <= 100; ++i) assert(seen[i]);
    assert(frog_jump_height(-50) == 0 && frog_jump_height(200) == FROG_MAX_HEIGHT);
    frog_model_init(&m);
    frog_model_input(&m, true, 0);
    voice(&m);
    frog_model_input(&m, false, 399);
    assert(m.phase == FROG_TOO_SHORT && m.score == -1);
    frog_model_input(&m, true, 500);
    int16_t silence[320] = {0};
    for (int i = 0; i < 20; ++i) frog_model_pcm(&m, silence, 320);
    frog_model_input(&m, false, 1000);
    assert(m.phase == FROG_TOO_QUIET);
    // Constant DC is not voice, even at full amplitude.
    frog_model_input(&m, true, 2000);
    for (int i = 0; i < 320; ++i) silence[i] = INT16_MIN;
    for (int i = 0; i < 20; ++i) frog_model_pcm(&m, silence, 320);
    frog_model_input(&m, false, 2500);
    assert(m.phase == FROG_TOO_QUIET);
    // Timeout scores once. A key still held cannot start another round.
    frog_model_input(&m, true, 3000);
    voice(&m);
    frog_model_tick(&m, 13000, 0);
    assert(m.phase == FROG_THINKING);
    frog_model_tick(&m, 13550, UINT32_MAX);
    frog_model_tick(&m, 14650, 0);
    frog_model_input(&m, true, 15000);
    assert(m.phase == FROG_RESULT && m.score == 100);
    frog_model_input(&m, false, 15100);
    frog_model_input(&m, true, 15200);
    assert(m.phase == FROG_LISTENING);
    frog_model_error(&m);
    frog_model_tick(&m, 50000, UINT32_MAX);
    frog_model_input(&m, true, 50100);
    assert(m.phase == FROG_AUDIO_ERROR && m.score == -1);
    frog_model_input(&m, false, 50200);
    frog_model_input(&m, true, 50300);
    assert(m.phase == FROG_LISTENING);
    frog_model_reset(&m);
    assert(m.phase == FROG_LISTENING);
    // Rapid press/release and presses during animations never create phantom rounds.
    frog_model_input(&m, false, 50301);
    assert(m.phase == FROG_TOO_SHORT);
    round_at(&m, 51000, 0);
    m.phase = FROG_THINKING;
    frog_model_input(&m, true, 54000);
    assert(m.phase == FROG_THINKING);
    m.phase = FROG_INPUT_ERROR;
    frog_model_input(&m, false, 55000);
    frog_model_reset(&m);
    assert(m.phase == FROG_INPUT_ERROR);
    puts("Frog model: PASS (101 scores, timing, silence, retries, bounds, wraparound)");
}
