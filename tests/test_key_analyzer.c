#include "key_analyzer.h"
#include "key_control.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static key_workspace_t work;
static int16_t pcm[PITCH_FRAME_SAMPLES];
static uint32_t noise_state = 12345;
static float noise(void) {
    noise_state = noise_state * 1664525u + 1013904223u;
    return (float)(noise_state >> 16) / 32768 - 1;
}
static void tone(float midi, float amplitude, float harmonics, float dc) {
    float hz = 440 * exp2f((midi - 69) / 12);
    for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) {
        float p = 6.28318530718f * hz * i / PITCH_SAMPLE_RATE;
        pcm[i] = (int16_t)(dc + amplitude * (sinf(p) + harmonics * sinf(2 * p)) + noise() * 40);
    }
}
static void feed_note(key_session_t *s, int note, unsigned frames) {
    tone(note, 6500, 0.35f, 1500);
    for (unsigned i = 0; i < frames; ++i) key_session_feed(s, &work, pcm);
}
static void profiles(void) {
    const float p[2][12] = {
        {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f},
        {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f},
    };
    for (int mode = 0; mode < 2; ++mode) for (int root = 0; root < 12; ++root) {
        float c[12];
        for (int i = 0; i < 12; ++i) c[i] = p[mode][(i - root + 12) % 12] * 3.7f;
        int a,b; float x,y;
        key_rank(c, &a, &b, &x, &y);
        assert(a == mode * 12 + root && b >= 0 && b != a);
        assert(x > .999f && x > y);
    }
    float flat[12] = {0}; int a,b; float x,y;
    key_rank(flat, &a, &b, &x, &y); assert(a == -1 && b == -1);
}
static void melody(int transpose, key_mode_t mode) {
    /* Tonal phrase: C E G E F D G B C, durations emphasize tonic/dominant. */
    const int notes[] = {60,64,67,64,65,62,67,71,72};
    const unsigned duration[] = {12,7,10,5,5,5,10,4,14};
    key_session_t s; key_session_begin(&s, mode);
    for (unsigned i = 0; i < sizeof(notes)/sizeof(notes[0]); ++i)
        feed_note(&s, notes[i] + transpose, duration[i]);
    key_session_finish(&s);
    if (s.first != transpose % 12) fprintf(stderr, "melody mode=%d transpose=%d got=%d phase=%d score=%f gap=%f\n", mode, transpose, s.first, s.phase, s.score, s.score-s.runner_score);
    assert(s.first == transpose % 12);
    assert(s.phase == KEY_RESULT || s.phase == KEY_AMBIGUOUS);
}
static void chords(void) {
    const int progression[][3] = {{60,64,67},{65,69,72},{67,71,74},{60,64,67}};
    key_session_t s; key_session_begin(&s, KEY_MUSIC);
    for (unsigned chord = 0; chord < 4; ++chord) {
        for (unsigned i = 0; i < PITCH_FRAME_SAMPLES; ++i) {
            float v = 0;
            for (unsigned note = 0; note < 3; ++note) {
                float hz = 440 * exp2f((progression[chord][note] - 69.0f) / 12);
                float p = 6.28318530718f * hz * i / PITCH_SAMPLE_RATE;
                v += 4000 * sinf(p) + 800 * sinf(2 * p);
            }
            pcm[i] = v;
        }
        unsigned frames = chord == 0 || chord == 3 ? 20 : 8;
        for (unsigned i = 0; i < frames; ++i) key_session_feed(&s, &work, pcm);
    }
    key_session_finish(&s);
    printf("Synthetic chord cadence: key=%d alternate=%d similarity=%.3f\n", s.first, s.second, s.score);
    assert(s.first == 0 && (s.phase == KEY_RESULT || s.phase == KEY_AMBIGUOUS));
}
static void minor_melody(int transpose, key_mode_t mode) {
    const int notes[] = {57,60,64,60,62,59,64,68,69};
    const unsigned duration[] = {12,7,10,5,5,5,10,4,14};
    key_session_t s; key_session_begin(&s, mode);
    for (unsigned i = 0; i < sizeof(notes)/sizeof(notes[0]); ++i)
        feed_note(&s, notes[i] + transpose, duration[i]);
    key_session_finish(&s);
    int expected = 12 + (9 + transpose) % 12;
    if (s.first != expected) fprintf(stderr, "minor mode=%d transpose=%d got=%d expected=%d\n",mode,transpose,s.first,expected);
    assert(s.first == expected);
}
static void failures(void) {
    for (int mode = KEY_HUM; mode <= KEY_MUSIC; ++mode) {
        key_session_t s; key_session_begin(&s, mode);
        key_session_finish(&s); assert(s.phase == KEY_SHORT);
        key_session_begin(&s, mode);
        memset(pcm, 0, sizeof(pcm));
        for (int i = 0; i < 30; ++i) key_session_feed(&s, &work, pcm);
        key_session_finish(&s); assert(s.phase == KEY_QUIET);
        key_session_begin(&s, mode);
        for (int i = 0; i < 40; ++i) {
            for (unsigned j = 0; j < PITCH_FRAME_SAMPLES; ++j) pcm[j] = noise() * 7000;
            key_session_feed(&s, &work, pcm);
        }
        key_session_finish(&s); assert(s.phase == KEY_QUIET);
        key_session_begin(&s, mode); feed_note(&s, 69, 30);
        key_session_finish(&s); assert(s.phase == KEY_FEW_NOTES);
        key_session_begin(&s, mode);
        for (unsigned j = 0; j < PITCH_FRAME_SAMPLES; ++j) pcm[j] = j % 32 < 16 ? 32767 : -32768;
        for (int i = 0; i < 30; ++i) key_session_feed(&s, &work, pcm);
        key_session_finish(&s); assert(s.phase == KEY_CLIPPED);
        key_session_begin(&s, mode); feed_note(&s, 60, PITCH_MAX_FRAMES + 10);
        assert(s.limit_reached && s.frames == PITCH_MAX_FRAMES);
        unsigned frames = s.frames;
        key_session_finish(&s); key_session_feed(&s, &work, pcm);
        assert(s.frames == frames);
        key_session_begin(&s, mode); assert(s.frames == 0 && s.usable == 0 && s.chroma[0] == 0);
    }
    key_session_t s; key_session_begin(&s, KEY_HUM);
    s.frames = s.usable = 60;
    for (unsigned i=0; i<12; ++i) s.chroma[i] = 5;
    key_session_finish(&s); assert(s.phase == KEY_AMBIGUOUS && s.first == -1);
}
static void controls(void) {
    key_input_t in = {0};
    key_input_apply(&in, KEY_INPUT_PRESS); assert(in.held && in.capture && in.generation == 1);
    key_input_apply(&in, KEY_INPUT_PRESS); assert(in.generation == 1);
    key_input_apply(&in, KEY_INPUT_RELEASE); assert(!in.held && in.capture);
    key_input_apply(&in, KEY_INPUT_PRESS); assert(in.held && in.generation == 2);
    key_input_apply(&in, KEY_INPUT_RELEASE);
    key_input_apply(&in, KEY_INPUT_MODE); assert(in.mode == KEY_MUSIC && !in.capture);
    /* A direct physical switch can emit DOWN/UP press before OK release. */
    for (int command = KEY_INPUT_MODE; command <= KEY_INPUT_CLEAR; ++command) {
        key_input_apply(&in, KEY_INPUT_PRESS);
        uint32_t generation = in.generation;
        key_input_apply(&in, command);
        assert(!in.held && !in.capture && in.generation != generation);
        generation = in.generation;
        key_input_apply(&in, KEY_INPUT_RELEASE);
        assert(!in.held && !in.capture && in.generation == generation);
    }
    for (int phase = KEY_READY; phase <= KEY_ERROR; ++phase) {
        key_session_t s; key_session_begin(&s, KEY_MUSIC);
        feed_note(&s, 60, 4);
        s.phase = phase; s.limit_reached = true;
        key_session_clear(&s, KEY_MUSIC);
        assert(s.phase == KEY_READY && s.mode == KEY_MUSIC);
        assert(s.frames == 0 && s.first == -1 && s.chroma[0] == 0 && !s.limit_reached);
    }
    in.generation = UINT32_MAX;
    key_input_apply(&in, KEY_INPUT_PRESS); assert(in.generation == 0 && in.held);
}
int main(void) {
    for (int midi = 36; midi <= 84; ++midi) {
        tone(midi + .1f, 7500, .6f, 2000);
        pitch_frame_t f = pitch_detect(&work.pitch, pcm);
        if (!f.voiced || fabsf(f.midi - midi - .1f) >= .12f) fprintf(stderr, "pitch midi=%d got=%f hz=%f voiced=%d\n", midi, f.midi, f.hz, f.voiced);
        assert(f.voiced && fabsf(f.midi - midi - .1f) < .12f);
    }
    profiles();
    for (int root = 0; root < 12; ++root) {
        melody(root, KEY_HUM); melody(root, KEY_MUSIC);
        minor_melody(root, KEY_HUM); minor_melody(root, KEY_MUSIC);
    }
    chords(); failures(); controls();
    puts("Key analysis: PASS (49 pitches, 24 profiles, 48 PCM melodies, chords, rejection, controls)");
}
