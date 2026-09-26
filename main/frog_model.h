#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { FROG_MIN_MS = 400, FROG_MAX_MS = 10000, FROG_THINK_MS = 550,
       FROG_JUMP_MS = 1100, FROG_MAX_HEIGHT = 88 };
typedef enum {
    FROG_READY, FROG_LISTENING, FROG_THINKING, FROG_JUMPING, FROG_RESULT,
    FROG_TOO_SHORT, FROG_TOO_QUIET, FROG_AUDIO_ERROR, FROG_INPUT_ERROR
} frog_phase_t;
typedef struct {
    frog_phase_t phase;
    uint32_t started_ms, phase_ms, samples, audible_samples;
    unsigned level;
    int score, best;
    bool held;
} frog_model_t;

void frog_model_init(frog_model_t *m);
/* Only the application worker owns the model. A held key cannot retrigger a
 * timed-out/error round; release and a new press are required. Time wraps safely. */
void frog_model_input(frog_model_t *m, bool held, uint32_t now);
void frog_model_pcm(frog_model_t *m, const int16_t *pcm, size_t count);
void frog_model_tick(frog_model_t *m, uint32_t now, uint32_t random);
void frog_model_error(frog_model_t *m);
void frog_model_replay(frog_model_t *m, uint32_t now);
void frog_model_reset(frog_model_t *m);
int frog_score(uint32_t random);
int frog_jump_height(int score);
int frog_jump_offset(const frog_model_t *m, uint32_t now);
