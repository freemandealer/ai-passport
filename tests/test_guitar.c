#include "guitar_score.h"
#include "guitar_player.h"
#include "guitar_shapes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static guitar_library_t lib;

static void chord(const char *key, const char *token, const char *name, const char *degree)
{
    guitar_key_t k;
    guitar_chord_t c;
    char text[24];
    assert(guitar_parse_key(key, &k));
    assert(guitar_parse_chord(token, k, &c));
    guitar_chord_name(c, text, sizeof(text));
    if (strcmp(text, name)) fprintf(stderr, "%s %s: %s != %s\n", key, token, text, name);
    assert(!strcmp(text, name));
    guitar_degree_name(c, k, text, sizeof(text));
    assert(!strcmp(text, degree));
}

static void mapping(void)
{
    chord("C", "6", "Am", "vi");
    chord("C", "Am", "Am", "vi");
    chord("C", "vi", "Am", "vi");
    chord("G", "7", "F#dim", "viidim");
    chord("F", "4", "Bb", "IV");
    chord("Am", "1", "Am", "i");
    chord("Am", "2", "Bdim", "iidim");
    chord("Am", "3", "C", "III");
    chord("Am", "V7", "E7", "V7");
    chord("C", "ii7", "Dm7", "ii7");
    chord("C", "b7", "Bbdim", "bviidim");
    chord("C", "bVII", "Bb", "bVII");
    chord("C", "4maj7", "Fmaj7", "IVmaj7");
    chord("D", "b3maj", "F", "bIII");
    chord("F#", "7", "E#dim", "viidim");
    chord("Db", "2", "Ebm", "ii");
    chord("Bbm", "6", "Gb", "VI");
    chord("Dbm", "3", "Fb", "III");
    chord("C#", "#7", "B##dim", "#viidim");
    guitar_key_t k;
    assert(guitar_parse_key("C", &k));
    const char *bad[] = {"", "8", "0", "H", "Amgarbage", "1/3", "IIII", "iiI", "C/", "Cb#", "i9", "C9", "#"};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        guitar_chord_t c;
        assert(!guitar_parse_chord(bad[i], k, &c));
    }
    const char *keys[] = {"C", "C#", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
                          "Cm", "C#m", "Dm", "Ebm", "Em", "Fm", "F#m", "Gm", "G#m", "Am", "Bbm", "Bm", "Cb", "Cbm", "B#", "B#m", "Dbm"};
    for (unsigned i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        assert(guitar_parse_key(keys[i], &k));
        for (char d = '1'; d <= '7'; ++d) {
            char token[] = {d, 0}, name[24], roman[24];
            guitar_chord_t c, roundtrip;
            assert(guitar_parse_chord(token, k, &c));
            guitar_chord_name(c, name, sizeof(name));
            guitar_degree_name(c, k, roman, sizeof(roman));
            assert(guitar_parse_chord(name, k, &roundtrip));
            assert(!memcmp(&c, &roundtrip, sizeof(c)));
            assert(guitar_parse_chord(roman, k, &roundtrip));
            assert(!memcmp(&c, &roundtrip, sizeof(c)));
        }
        for (char root = 'A'; root <= 'G'; ++root) for (int acc = -1; acc <= 1; ++acc) {
            char token[12], degree[24];
            snprintf(token, sizeof(token), "%c%s7", root, acc < 0 ? "b" : acc > 0 ? "#" : "");
            guitar_chord_t c, back;
            assert(guitar_parse_chord(token, k, &c));
            guitar_degree_name(c, k, degree, sizeof(degree));
            assert(guitar_parse_chord(degree, k, &back));
            assert(!memcmp(&c, &back, sizeof(c)));
        }
    }
}

static void parsing(void)
{
    guitar_error_t e;
    assert(guitar_parse(GUITAR_DEFAULT_SCORE, strlen(GUITAR_DEFAULT_SCORE), &lib, &e));
    assert(lib.song_count == 2 && lib.bar_count == 16);
    assert(lib.songs[1].first_bar == 8 && lib.songs[1].key.minor);
    const char *valid = "\xEF\xBB\xBF\r\n---\r\n C \r\n Dm \r\n 95 \r\n1\t4  V7\r\n\r\n---\n另一个歌名\nBb\n30\nBb Eb F7 Bb";
    assert(guitar_parse(valid, strlen(valid), &lib, &e));
    assert(lib.song_count == 2 && lib.bar_count == 7 && !strcmp(lib.songs[0].title, "C"));
    const char *bad[] = {"", "C", "---\nname\nC\n20\nC", "---\nname\nC\n121\nC", "---\nname\nC\n80bpm\nC",
        "---\nname\nHm\n80\nC", "---\nname\nC\n80\nH", "---\nname\nC\n80\n---\na\nC\n80\nC",
        "---\n\nC\n80\nC", "---\nname\nC\n80", "---\nname\nC", "---\nname\nC\n80\nC\n---"};
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        assert(!guitar_parse(bad[i], strlen(bad[i]), &lib, &e));
        assert(e.code != G_PARSE_OK);
    }
    assert(!guitar_parse("---\nname\nC\n80\nC\0D", 19, &lib, &e));
    assert(!guitar_title_valid("\xE4\xB8"));
    assert(!guitar_title_valid("\xED\xA0\x80"));
    assert(!guitar_title_valid("guitar🎸"));
    assert(!guitar_title_valid("\xE9\xBF\xB0")); /* U+9FF0 outside the font */
    assert(guitar_title_valid("晴天（练习）"));
    char limits[GUITAR_TEXT_MAX + 1] = "---\na\nC\n80\n";
    for (int i = 0; i < GUITAR_BAR_MAX; ++i) strcat(limits, "C ");
    assert(guitar_parse(limits, strlen(limits), &lib, &e));
    strcat(limits, "C");
    assert(!guitar_parse(limits, strlen(limits), &lib, &e) && e.code == G_PARSE_BAR_LIMIT);
    limits[0] = 0;
    for (int i = 0; i <= GUITAR_SONG_MAX; ++i) strcat(limits, "---\na\nC\n80\nC\n");
    assert(!guitar_parse(limits, strlen(limits), &lib, &e) && e.code == G_PARSE_SONG_LIMIT);
    const char *comments = "---\ncomments\nC\n80\nC(轻扫) 6 (两拍 分解) G7 F\n";
    assert(guitar_parse(comments, strlen(comments), &lib, &e));
    assert(lib.bar_count == 4 && lib.songs[0].bar_count == 4);
    assert(!strcmp(guitar_chord_comment(&lib, lib.bars[0]), "轻扫"));
    assert(!strcmp(guitar_chord_comment(&lib, lib.bars[1]), "两拍 分解"));
    assert(!strcmp(guitar_chord_comment(&lib, lib.bars[2]), ""));
    char name[24]; guitar_chord_name(lib.bars[1], name, sizeof(name));
    assert(!strcmp(name, "Am"));
    const char *invalid_notes[] = {"(a) C", "C()", "C( )", "C(unclosed", "C(a(b))", "C(a)(b)", "C(a)G", "C(a\nb)", "C(🎸)"};
    for (unsigned i = 0; i < sizeof(invalid_notes)/sizeof(invalid_notes[0]); ++i) {
        snprintf(limits, sizeof(limits), "---\nnote\nC\n80\n%s", invalid_notes[i]);
        assert(!guitar_parse(limits, strlen(limits), &lib, &e) && e.code == G_PARSE_COMMENT && e.line == 5);
    }
    snprintf(limits, sizeof(limits), "---\nlimit\nC\n80\nC(%s)", "一二三四五六七八九十一二三四五六七八九十一二三四");
    assert(guitar_parse(limits, strlen(limits), &lib, &e));
    snprintf(limits, sizeof(limits), "---\nlimit\nC\n80\nC(%s)", "一二三四五六七八九十一二三四五六七八九十一二三四五");
    assert(!guitar_parse(limits, strlen(limits), &lib, &e) && e.code == G_PARSE_COMMENT);
}

static void shapes(void)
{
    const int tuning[] = {4,9,2,7,11,4};
    for (unsigned root = 0; root < 12; ++root) for (unsigned q = 0; q < G_QUALITY_COUNT; ++q) {
        guitar_shape_t s = guitar_shape((guitar_chord_t){.pitch = root, .quality = q});
        uint16_t heard = 0;
        bool first = true;
        for (int i = 0; i < 6; ++i) {
            if (s.frets[i] < 0) continue;
            int tone = (tuning[i] + s.frets[i] + 12 - root) % 12;
            if (first) { assert(tone == 0); first = false; }
            heard |= 1u << tone;
            assert(s.frets[i] == 0 || (s.frets[i] >= s.first_fret && s.frets[i] < s.first_fret + 5));
        }
        uint16_t expected = guitar_chord_intervals(q);
        assert((heard & ~expected) == 0);
        /* Conventional C7 omits the fifth; root, third and seventh remain. */
        assert((heard | (1u << 7)) == (expected | (1u << 7)));
    }
}

static void timing(void)
{
    assert(guitar_parse(GUITAR_DEFAULT_SCORE, strlen(GUITAR_DEFAULT_SCORE), &lib, NULL));
    guitar_player_t p;
    guitar_player_init(&p, &lib);
    p.bpm = 60;
    const uint64_t start = UINT64_C(9000000000);
    guitar_player_action(&p, &lib, G_OK, start);
    for (unsigned step = 0; step < 16; ++step) {
        guitar_player_tick(&p, &lib, start + step * 500000);
        assert(p.eighth == step % 8 && p.bar == 0);
        assert(p.transport == (step < 8 ? G_COUNT_IN : G_PLAYING));
    }
    guitar_player_tick(&p, &lib, start + 8000000);
    assert(p.bar == 1);
    guitar_player_action(&p, &lib, G_OK, start + 9100000);
    assert(p.bar == 1 && p.transport == G_PAUSED);
    guitar_player_action(&p, &lib, G_OK, start + 12000000);
    assert(p.bar == 1 && p.transport == G_COUNT_IN);
    guitar_player_tick(&p, &lib, start + 16000000);
    assert(p.bar == 1 && p.transport == G_PLAYING && p.eighth == 0);
    guitar_player_tick(&p, &lib, start + 44000000);
    assert(p.transport == G_FINISHED && p.bar == 7);
    guitar_player_action(&p, &lib, G_OK, start + 45000000);
    assert(p.transport == G_COUNT_IN && p.bar == 0);
    guitar_player_action(&p, &lib, G_DOWN_DOUBLE, start + 45000001);
    assert(p.page == G_SCORE_PAGE && p.bar == 2);
    guitar_player_action(&p, &lib, G_DOWN_LONG, start + 45000002);
    assert(p.page == G_TEMPO_PAGE && p.transport == G_PAUSED);
    assert(p.candidate_bpm == 60);
    guitar_player_action(&p, &lib, G_UP, start);
    assert(p.candidate_bpm == 61);
    guitar_player_action(&p, &lib, G_UP_DOUBLE, start);
    assert(p.candidate_bpm == 63);
    guitar_player_action(&p, &lib, G_DOWN, start);
    assert(p.candidate_bpm == 62);
    guitar_player_action(&p, &lib, G_DOWN_DOUBLE, start);
    assert(p.candidate_bpm == 60);
    for (int i = 0; i < 100; ++i) guitar_player_action(&p, &lib, G_UP, start);
    assert(p.candidate_bpm == 120);
    for (int i = 0; i < 100; ++i) guitar_player_action(&p, &lib, G_DOWN, start);
    assert(p.candidate_bpm == 30);
    guitar_player_action(&p, &lib, G_OK, start);
    assert(p.page == G_SCORE_PAGE && p.bpm == 30);
    p.bpm = 83;
    guitar_player_action(&p, &lib, G_DOWN_LONG, start);
    assert(p.candidate_bpm == 83); /* Opening settings must not round tempo. */
    guitar_player_action(&p, &lib, G_UP, start);
    guitar_player_action(&p, &lib, G_OK, start);
    assert(p.bpm == 84);
    guitar_player_action(&p, &lib, G_UP_LONG, start);
    assert(p.page == G_VOLUME_PAGE && p.volume == 5 && p.transport == G_PAUSED);
    for (int i = 0; i < 20; ++i) guitar_player_action(&p, &lib, G_UP, start);
    assert(p.candidate_volume == 10 && p.volume == 5);
    for (int i = 0; i < 20; ++i) guitar_player_action(&p, &lib, G_DOWN, start);
    assert(p.candidate_volume == 0);
    guitar_player_action(&p, &lib, G_OK, start);
    assert(p.page == G_SCORE_PAGE && p.volume == 0);
    guitar_player_action(&p, &lib, G_OK_LONG, start);
    guitar_player_action(&p, &lib, G_DOWN, start);
    guitar_player_action(&p, &lib, G_OK, start);
    assert(p.song == 1 && p.bar == 0 && p.bpm == 70);
    assert(p.volume == 0);
    guitar_player_action(&p, &lib, G_DOWN, start);
    assert(p.bar == 1);
    guitar_player_action(&p, &lib, G_UP_DOUBLE, start);
    assert(p.bar == 0);
    /* Non-divisible BPM boundary: advance exactly at ceil(8*30e6/BPM). */
    p.bpm = 95;
    guitar_player_action(&p, &lib, G_OK, start);
    guitar_player_tick(&p, &lib, start + 2526315);
    assert(p.transport == G_COUNT_IN);
    guitar_player_tick(&p, &lib, start + 2526316);
    assert(p.transport == G_PLAYING);
    guitar_player_action(&p, &lib, G_DOWN, start + 2600000);
    assert(p.transport == G_PLAYING && p.bar == 1 && p.eighth == 0);
    guitar_player_tick(&p, &lib, start + 2600000 + 2526316);
    assert(p.bar == 2);
    guitar_player_action(&p, &lib, G_UP_DOUBLE, start + 5300000);
    assert(p.transport == G_PLAYING && p.bar == 0);
    guitar_player_tick(&p, &lib, start + UINT64_C(864000000000));
    assert(p.transport == G_FINISHED);
    for (unsigned bpm = 30; bpm <= 120; ++bpm) {
        guitar_player_init(&p, &lib); p.bpm = bpm;
        guitar_player_action(&p, &lib, G_OK, start);
        for (unsigned step = 1; step < 24; ++step) {
            uint64_t boundary = (step * UINT64_C(30000000) + bpm - 1) / bpm;
            guitar_player_tick(&p, &lib, start + boundary - 1);
            assert(p.eighth == (step - 1) % 8);
            guitar_player_tick(&p, &lib, start + boundary);
            assert(p.eighth == step % 8);
        }
    }
}

static void malformed_inputs(void)
{
    uint32_t random = 1;
    char input[GUITAR_TEXT_MAX + 1];
    for (unsigned i = 0; i < 10000; ++i) {
        size_t n = i % sizeof(input);
        for (size_t j = 0; j < n; ++j) {
            random = random * 1664525u + 1013904223u;
            input[j] = (char)(random >> 24);
        }
        if (i % 2 && n > 16) memcpy(input, "---\ntest\nC\n80\n", 14);
        guitar_error_t error;
        (void)guitar_parse(input, n, &lib, &error);
    }
}

int main(void)
{
    mapping(); parsing(); shapes(); timing(); malformed_inputs();
    puts("Guitar parser, key mapping, 180 voicings and transport: PASS");
    return 0;
}
