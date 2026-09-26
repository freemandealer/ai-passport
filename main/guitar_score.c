#include "guitar_score.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

const char GUITAR_DEFAULT_SCORE[] =
    "---\n绿光练习\nC\n80\nC C Am Am F F G G\n"
    "---\n小调漫步\nAm\n70\n1 4 5 1 6 3 7 1\n";

static const char LETTERS[] = "CDEFGAB";
static const uint8_t NATURAL[] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t SCALES[2][7] = {{0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}};
static const uint8_t QUALITIES[2][7] = {
    {G_MAJOR, G_MINOR, G_MINOR, G_MAJOR, G_MAJOR, G_MINOR, G_DIM},
    {G_MINOR, G_DIM, G_MAJOR, G_MINOR, G_MINOR, G_MAJOR, G_MAJOR}
};
static const char *const SUFFIXES[] = {
    "", "m", "7", "maj7", "m7", "dim", "dim7", "m7b5", "aug", "sus2", "sus4", "5", "6", "m6", "add9"
};

static int mod12(int n) { return (n % 12 + 12) % 12; }
static int delta(int n) { n = mod12(n); return n > 6 ? n - 12 : n; }

static bool note(const char **cursor, uint8_t *pitch, uint8_t *letter)
{
    const char *p = *cursor;
    if (!*p) return false;
    const char *l = strchr(LETTERS, *p);
    if (!l) return false;
    *letter = (uint8_t)(l - LETTERS);
    int v = NATURAL[*letter];
    ++p;
    char sign = *p;
    if (sign == '#' || sign == 'b')
        for (unsigned n = 0; n < 6 && *p == sign; ++n, ++p) v += sign == '#' ? 1 : -1;
    *pitch = (uint8_t)mod12(v);
    *cursor = p;
    return true;
}

bool guitar_parse_key(const char *text, guitar_key_t *key)
{
    guitar_key_t k = {0};
    const char *start = text;
    if (!note(&text, &k.pitch, &k.letter)) return false;
    if (text - start > 2) return false;
    if (*text == 'm') { k.minor = true; ++text; }
    if (*text) return false;
    *key = k;
    return true;
}

static bool quality(const char *p, uint8_t *q)
{
    for (unsigned i = 0; i < G_QUALITY_COUNT; ++i) {
        if (strcmp(p, SUFFIXES[i]) == 0) { *q = i; return true; }
    }
    if (!strcmp(p, "maj") || !strcmp(p, "M")) { *q = G_MAJOR; return true; }
    if (!strcmp(p, "min")) { *q = G_MINOR; return true; }
    if (!strcmp(p, "M7")) { *q = G_MAJ7; return true; }
    return false;
}

bool guitar_parse_chord(const char *text, guitar_key_t key, guitar_chord_t *chord)
{
    if (!text || !*text || key.pitch >= 12 || key.letter >= 7) return false;
    guitar_chord_t c = {0};
    const char *p = text;
    if (note(&p, &c.pitch, &c.letter)) {
        if (!quality(p, &c.quality)) return false;
        *chord = c;
        return true;
    }

    int accidental = 0, degree = -1;
    p = text;
    char sign = *p;
    if (sign == 'b' || sign == '#')
        for (unsigned n = 0; n < 6 && *p == sign; ++n, ++p) accidental += sign == '#' ? 1 : -1;
    bool roman = false, lower = false;
    if (*p >= '1' && *p <= '7') degree = *p++ - '1';
    else {
        static const char *const numerals[] = {"VII", "III", "VI", "IV", "II", "V", "I"};
        static const uint8_t values[] = {6, 2, 5, 3, 1, 4, 0};
        lower = *p == 'i' || *p == 'v';
        for (unsigned i = 0; i < 7; ++i) {
            size_t n = strlen(numerals[i]), j = 0;
            while (j < n && p[j] && p[j] == (lower ? tolower(numerals[i][j]) : numerals[i][j])) ++j;
            if (j == n) { degree = values[i]; p += n; roman = true; break; }
        }
    }
    if (degree < 0) return false;
    c.pitch = (uint8_t)mod12(key.pitch + SCALES[key.minor][degree] + accidental);
    c.letter = (key.letter + degree) % 7;
    c.quality = roman ? (lower ? G_MINOR : G_MAJOR) : QUALITIES[key.minor][degree];
    if (*p) {
        if (!quality(p, &c.quality)) return false;
        /* A lower-case Roman seventh means a minor seventh (ii7, vi7). */
        if (roman && lower && !strcmp(p, "7")) c.quality = G_MIN7;
        if (roman && lower && !strcmp(p, "6")) c.quality = G_MIN6;
    }
    *chord = c;
    return true;
}

static const char *accidental_text(int d)
{
    static const char flats[] = "bbbbbb", sharps[] = "######";
    return d < 0 ? &flats[6 + d] : &sharps[6 - d];
}

static void spelling(uint8_t pitch, uint8_t letter, char *out, size_t n)
{
    int d = delta(pitch - NATURAL[letter]);
    snprintf(out, n, "%c%s", LETTERS[letter], accidental_text(d));
}

void guitar_chord_name(guitar_chord_t c, char *out, size_t n)
{
    char root[8];
    spelling(c.pitch, c.letter, root, sizeof(root));
    snprintf(out, n, "%s%s", root, SUFFIXES[c.quality]);
}

void guitar_key_name(guitar_key_t k, char *out, size_t n)
{
    char root[8];
    spelling(k.pitch, k.letter, root, sizeof(root));
    snprintf(out, n, "%s%s", root, k.minor ? "m" : "");
}

void guitar_degree_name(guitar_chord_t c, guitar_key_t k, char *out, size_t n)
{
    static const char *const upper[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char *const lower[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    unsigned d = (c.letter + 7 - k.letter) % 7;
    int a = delta(c.pitch - k.pitch - SCALES[k.minor][d]);
    bool minor = c.quality == G_MINOR || c.quality == G_MIN7 || c.quality == G_MIN6 ||
                 c.quality == G_DIM || c.quality == G_DIM7 || c.quality == G_HALF_DIM;
    const char *suffix = SUFFIXES[c.quality];
    if (c.quality == G_MINOR) suffix = "";
    if (c.quality == G_MIN7) suffix = "7";
    if (c.quality == G_MIN6) suffix = "6";
    if (c.quality == G_HALF_DIM) suffix = "7b5";
    snprintf(out, n, "%s%s%s", accidental_text(a),
             minor ? lower[d] : upper[d], suffix);
}

bool guitar_title_valid(const char *text)
{
    const unsigned char *p = (const unsigned char *)text;
    unsigned count = 0;
    while (*p) {
        uint32_t cp = *p++;
        if (cp >= 0x80) {
            if (cp < 0xE0 || cp > 0xEF || !p[0] || !p[1] ||
                (p[0] & 0xC0) != 0x80 || (p[1] & 0xC0) != 0x80) return false;
            cp = ((cp & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
            p += 2;
            if (cp < 0x800) return false;
        }
        if (!((cp >= 0x20 && cp <= 0x7E) || (cp >= 0x4E00 && cp <= GUITAR_CJK_LAST) ||
              cp == 0x3002 || cp == 0x300A || cp == 0x300B || cp == 0xFF08 ||
              cp == 0xFF09 || cp == 0xFF0C || cp == 0xFF01 || cp == 0xFF1F)) return false;
        if (++count > 24) return false;
    }
    return count > 0;
}

static bool fail(guitar_error_t *e, guitar_parse_code_t code, unsigned line, const char *token)
{
    if (e) {
        *e = (guitar_error_t){.code = code, .line = line};
        snprintf(e->token, sizeof(e->token), "%.31s", token ? token : "");
    }
    return false;
}

const char *guitar_chord_comment(const guitar_library_t *lib, guitar_chord_t chord)
{
    return chord.comment <= GUITAR_TEXT_MAX ? lib->comments + chord.comment : "";
}

bool guitar_parse(const char *text, size_t length, guitar_library_t *out, guitar_error_t *error)
{
    if (!text || !out || length == 0 || length > GUITAR_TEXT_MAX) return fail(error, G_PARSE_SIZE, 1, "");
    if (memchr(text, 0, length)) return fail(error, G_PARSE_CHORD, 1, "NUL");
    memset(out, 0, sizeof(*out));
    if (error) *error = (guitar_error_t){0};
    unsigned line = 0, field = 0;
    size_t comment_used = 1;
    guitar_song_t *song = NULL;
    size_t pos = 0;
    if (length >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3)) pos = 3;
    while (pos < length) {
        ++line;
        size_t start = pos;
        while (pos < length && text[pos] != '\n' && text[pos] != '\r') ++pos;
        size_t end = pos;
        if (pos < length && text[pos++] == '\r' && pos < length && text[pos] == '\n') ++pos;
        while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
        while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t')) --end;
        size_t len = end - start;
        if (len == 3 && !memcmp(text + start, "---", 3)) {
            if (song && (field != 3 || !song->bar_count)) return fail(error, G_PARSE_EMPTY, line, "");
            if (out->song_count == GUITAR_SONG_MAX) return fail(error, G_PARSE_SONG_LIMIT, line, "");
            song = &out->songs[out->song_count++];
            song->first_bar = out->bar_count;
            field = 0;
            continue;
        }
        if (!song) {
            if (!len) continue;
            return fail(error, G_PARSE_SEPARATOR, line, "");
        }
        if (field < 3) {
            char header[GUITAR_TITLE_MAX];
            if (!len || len >= sizeof(header)) return fail(error, G_PARSE_HEADER, line, "");
            memcpy(header, text + start, len); header[len] = 0;
            if (field == 0) {
                if (!guitar_title_valid(header)) return fail(error, G_PARSE_TITLE, line, "");
                memcpy(song->title, header, len + 1);
            } else if (field == 1) {
                if (!guitar_parse_key(header, &song->key)) return fail(error, G_PARSE_KEY, line, header);
            } else {
                unsigned bpm = 0;
                if (len > 3) return fail(error, G_PARSE_BPM, line, header);
                for (size_t i = 0; i < len; ++i) {
                    if (header[i] < '0' || header[i] > '9') return fail(error, G_PARSE_BPM, line, header);
                    bpm = bpm * 10 + (unsigned)(header[i] - '0');
                }
                if (bpm < 30 || bpm > 120) return fail(error, G_PARSE_BPM, line, header);
                song->bpm = bpm;
            }
            ++field;
            continue;
        }
        while (start < end) {
            if (text[start] == '(') {
                if (!song->bar_count || out->bars[out->bar_count - 1].comment)
                    return fail(error, G_PARSE_COMMENT, line, "");
                size_t finish = start + 1;
                while (finish < end && text[finish] != ')') {
                    if (text[finish] == '(') return fail(error, G_PARSE_COMMENT, line, "");
                    ++finish;
                }
                if (finish == end || (finish + 1 < end && text[finish + 1] != ' ' && text[finish + 1] != '\t'))
                    return fail(error, G_PARSE_COMMENT, line, "");
                size_t first = start + 1, last = finish;
                while (first < last && (text[first] == ' ' || text[first] == '\t')) ++first;
                while (last > first && (text[last - 1] == ' ' || text[last - 1] == '\t')) --last;
                size_t n = last - first;
                if (!n || n >= GUITAR_COMMENT_MAX || comment_used + n + 1 > sizeof(out->comments))
                    return fail(error, G_PARSE_COMMENT, line, "");
                memcpy(out->comments + comment_used, text + first, n);
                out->comments[comment_used + n] = 0;
                if (!guitar_title_valid(out->comments + comment_used)) return fail(error, G_PARSE_COMMENT, line, "");
                out->bars[out->bar_count - 1].comment = comment_used;
                comment_used += n + 1;
                start = finish + 1;
                while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
                continue;
            }
            size_t finish = start;
            while (finish < end && text[finish] != ' ' && text[finish] != '\t' && text[finish] != '(') ++finish;
            char token[32];
            size_t size = finish - start;
            if (size >= sizeof(token)) return fail(error, G_PARSE_CHORD, line, "token too long");
            memcpy(token, text + start, size); token[size] = 0;
            if (out->bar_count >= GUITAR_BAR_MAX) return fail(error, G_PARSE_BAR_LIMIT, line, token);
            if (!guitar_parse_chord(token, song->key, &out->bars[out->bar_count])) return fail(error, G_PARSE_CHORD, line, token);
            ++out->bar_count; ++song->bar_count;
            start = finish;
            while (start < end && (text[start] == ' ' || text[start] == '\t')) ++start;
        }
    }
    if (!song || field != 3 || !song->bar_count) return fail(error, G_PARSE_EMPTY, line, "");
    return true;
}
