#include "guitar_score.h"
#include <stdio.h>
static char input[GUITAR_TEXT_MAX + 1];
static guitar_library_t library;
int main(void)
{
    size_t n = fread(input, 1, sizeof(input), stdin);
    guitar_error_t error;
    if (!guitar_parse(input, n, &library, &error)) {
        printf("%u %u\n", error.code, error.line);
        return 1;
    }
    printf("%u %u\n", library.song_count, library.bar_count);
    return 0;
}
