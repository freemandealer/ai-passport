#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GUITAR_TEXT_MAX 4096
#define GUITAR_SONG_MAX 16
#define GUITAR_BAR_MAX 512
#define GUITAR_TITLE_MAX 73
#define GUITAR_COMMENT_MAX 73
#define GUITAR_CJK_LAST 0x9FEF

typedef enum {
    G_MAJOR, G_MINOR, G_7, G_MAJ7, G_MIN7, G_DIM, G_DIM7, G_HALF_DIM,
    G_AUG, G_SUS2, G_SUS4, G_POWER, G_6, G_MIN6, G_ADD9, G_QUALITY_COUNT
} guitar_quality_t;

typedef struct { uint8_t pitch, letter; bool minor; } guitar_key_t;
typedef struct { uint8_t pitch, letter, quality; uint16_t comment; } guitar_chord_t;
typedef struct {
    char title[GUITAR_TITLE_MAX];
    guitar_key_t key;
    uint16_t bpm, first_bar, bar_count;
} guitar_song_t;
typedef struct {
    guitar_song_t songs[GUITAR_SONG_MAX];
    guitar_chord_t bars[GUITAR_BAR_MAX];
    char comments[GUITAR_TEXT_MAX + 1]; /* Offset zero means no annotation. */
    uint16_t song_count, bar_count;
} guitar_library_t;

typedef enum {
    G_PARSE_OK, G_PARSE_SIZE, G_PARSE_SEPARATOR, G_PARSE_TITLE, G_PARSE_KEY,
    G_PARSE_BPM, G_PARSE_CHORD, G_PARSE_EMPTY, G_PARSE_SONG_LIMIT,
    G_PARSE_BAR_LIMIT, G_PARSE_HEADER, G_PARSE_COMMENT
} guitar_parse_code_t;
typedef struct { guitar_parse_code_t code; unsigned line; char token[32]; } guitar_error_t;

/* Each chord is one four-beat bar; parenthesized annotations add no beats.
 * Output is scratch until
 * success; callers preserve the active library on failure. No heap or platform APIs. */
bool guitar_parse(const char *text, size_t length, guitar_library_t *out, guitar_error_t *error);
bool guitar_parse_key(const char *text, guitar_key_t *key);
bool guitar_parse_chord(const char *text, guitar_key_t key, guitar_chord_t *chord);
void guitar_chord_name(guitar_chord_t chord, char *out, size_t capacity);
void guitar_degree_name(guitar_chord_t chord, guitar_key_t key, char *out, size_t capacity);
void guitar_key_name(guitar_key_t key, char *out, size_t capacity);
bool guitar_title_valid(const char *text);
const char *guitar_chord_comment(const guitar_library_t *lib, guitar_chord_t chord);
extern const char GUITAR_DEFAULT_SCORE[];
