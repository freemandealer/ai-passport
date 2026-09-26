/* Acoustic regressions, deliberately distinct from the key-profile vectors.
 * --report allows a before/after run without accepting old failures as passes. */
#include "key_analyzer.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static key_workspace_t work;
static int16_t pcm[PITCH_FRAME_SAMPLES];
static unsigned passed, total;
static const int phrase[] = {0,4,7,4,5,2,7,11,12};
static const unsigned durations[] = {12,7,10,5,5,5,10,4,14};

static int run_phrase(int tonic, float detune, int timbre, int octave) {
    key_session_t session; key_session_begin(&session, KEY_MUSIC);
    double phase = 0;
    for (unsigned note = 0; note < sizeof(phrase)/sizeof(phrase[0]); ++note) {
        float hz = 440.0f * exp2f((octave + tonic + phrase[note] - 69 + detune / 100) / 12);
        for (unsigned frame = 0; frame < durations[note]; ++frame) {
            for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) {
                phase += 6.283185307179586 * hz / PITCH_SAMPLE_RATE;
                if (phase > 6.283185307179586) phase -= 6.283185307179586;
                float v;
                if (timbre == 1) {
                    v = 0.35f * sin(phase) + 0.5f * sin(2 * phase) + sin(3 * phase)
                        + 0.5f * sin(4 * phase) + 0.3f * sin(5 * phase);
                } else if (timbre == 2) {
                    v = sin(phase) + 0.5f * sin(2 * phase) + 0.33f * sin(3 * phase)
                        + 0.25f * sin(4 * phase) + 1.8f * sin(5 * phase);
                } else v = sin(phase);
                pcm[i] = (int16_t)(4500 * v);
            }
            key_session_feed(&session, &work, pcm);
        }
    }
    key_session_finish(&session);
    ++total;
    if (session.first == tonic) ++passed;
    else printf("Mismatch: tonic=%d cents=%.0f timbre=%d octave=%d result=%d phase=%d\n",
                tonic,detune,timbre,octave,session.first,session.phase);
    return session.first;
}

static void reject_old_overlap(void) {
    key_session_t s; key_session_begin(&s, KEY_MUSIC);
    for (unsigned i=0;i<PITCH_FRAME_SAMPLES;++i) pcm[i]=8000*sinf(6.2831853f*440*i/PITCH_SAMPLE_RATE);
    for (int frame=0;frame<4;++frame) key_session_feed(&s,&work,pcm);
    memset(pcm,0,sizeof(pcm));
    key_session_feed(&s,&work,pcm);
    unsigned usable=s.usable;
    for (unsigned i=0;i<PITCH_FRAME_SAMPLES;++i) pcm[i]=8000*sinf(6.2831853f*261.6256f*i/PITCH_SAMPLE_RATE);
    key_session_feed(&s,&work,pcm);
    assert(s.usable==usable); // First post-silence frame primes, never mixes old A.
    key_session_begin(&s,KEY_MUSIC);
    key_session_feed(&s,&work,pcm);
    assert(s.usable==0); // New session cannot reuse previous FFT audio either.
}

int main(int argc,char **argv) {
    bool report=argc==2 && strcmp(argv[1],"--report")==0;
    for (int tonic=0;tonic<12;++tonic) {
        run_phrase(tonic,42,0,60);
        run_phrase(tonic,-42,0,60);
        run_phrase(tonic,0,1,60);
        run_phrase(tonic,0,2,60);
        run_phrase(tonic,0,1,48);
        run_phrase(tonic,0,2,48); // Strong fifth partial: old E major -> G# minor.
        run_phrase(tonic,0,0,36); // C2..B3 bass range, previously mostly excluded.
    }
    printf("Music acoustic fixtures: %u/%u top-key matches (synthetic only)\n",passed,total);
    if (!report) {
        assert(passed==total);
        reject_old_overlap();
    }
}
