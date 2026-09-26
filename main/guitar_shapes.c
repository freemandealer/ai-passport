#include "guitar_shapes.h"

/* One voicing per quality, moved from the A-string root. All strings remain
 * chord tones; the UI marks X/O and absolute starting fret explicitly. */
static const int8_t A_SHAPES[G_QUALITY_COUNT][6] = {
    {-1,0,2,2,2,0}, {-1,0,2,2,1,0}, {-1,0,2,0,2,0}, {-1,0,2,1,2,0},
    {-1,0,2,0,1,0}, {-1,0,1,2,1,-1}, {-1,0,1,2,1,2}, {-1,0,1,0,1,-1},
    {-1,0,3,2,2,1}, {-1,0,2,2,0,0}, {-1,0,2,2,3,0}, {-1,0,2,2,-1,-1},
    {-1,0,2,2,2,2}, {-1,0,2,2,1,2}, {-1,0,2,4,2,0}
};

guitar_shape_t guitar_shape(guitar_chord_t chord)
{
    guitar_shape_t s = {.first_fret = 1};
    unsigned root_fret = (chord.pitch + 12 - 9) % 12;
    for (unsigned i = 0; i < 6; ++i)
        s.frets[i] = A_SHAPES[chord.quality][i] < 0 ? -1 : A_SHAPES[chord.quality][i] + root_fret;
    unsigned e_fret = (chord.pitch + 12 - 4) % 12;
    static const int8_t E_SHAPES[5][6] = {
        {0,2,2,1,0,0}, {0,2,2,0,0,0}, {0,2,0,1,0,0},
        {-1,-1,-1,-1,-1,-1}, {0,2,0,0,0,0}
    };
    if (e_fret < root_fret && chord.quality <= G_MIN7 && chord.quality != G_MAJ7)
        for (unsigned i = 0; i < 6; ++i) s.frets[i] = E_SHAPES[chord.quality][i] + e_fret;
    /* Prefer familiar open shapes and low-position E-family barre chords. */
    typedef struct { uint8_t pitch, quality; int8_t frets[6]; } open_shape_t;
    static const open_shape_t common[] = {
        {0,G_MAJOR,{-1,3,2,0,1,0}}, {2,G_MAJOR,{-1,-1,0,2,3,2}},
        {4,G_MAJOR,{0,2,2,1,0,0}}, {5,G_MAJOR,{1,3,3,2,1,1}},
        {7,G_MAJOR,{3,2,0,0,0,3}}, {2,G_MINOR,{-1,-1,0,2,3,1}},
        {4,G_MINOR,{0,2,2,0,0,0}}, {5,G_MINOR,{1,3,3,1,1,1}},
        {7,G_MINOR,{3,5,5,3,3,3}}, {0,G_7,{-1,3,2,3,1,0}},
        {2,G_7,{-1,-1,0,2,1,2}}, {4,G_7,{0,2,0,1,0,0}},
        {7,G_7,{3,2,0,0,0,1}}, {11,G_7,{-1,2,1,2,0,2}},
        {0,G_MAJ7,{-1,3,2,0,0,0}}, {5,G_MAJ7,{-1,-1,3,2,1,0}},
        {4,G_MIN7,{0,2,0,0,0,0}}, {2,G_MIN7,{-1,-1,0,2,1,1}},
        {2,G_SUS2,{-1,-1,0,2,3,0}}, {2,G_SUS4,{-1,-1,0,2,3,3}}
    };
    for (unsigned i = 0; i < sizeof(common) / sizeof(common[0]); ++i) {
        if (common[i].pitch != chord.pitch || common[i].quality != chord.quality) continue;
        for (unsigned j = 0; j < 6; ++j) s.frets[j] = common[i].frets[j];
        break;
    }
    unsigned highest = 0, lowest = 24;
    for (unsigned i = 0; i < 6; ++i) {
        if (s.frets[i] > 0 && (unsigned)s.frets[i] < lowest) lowest = s.frets[i];
        if (s.frets[i] > 0 && (unsigned)s.frets[i] > highest) highest = s.frets[i];
    }
    s.first_fret = highest <= 5 ? 1 : lowest;
    return s;
}

uint16_t guitar_chord_intervals(guitar_quality_t q)
{
    #define N(n) (1u << (n))
    static const uint16_t masks[] = {
        N(0)|N(4)|N(7), N(0)|N(3)|N(7), N(0)|N(4)|N(7)|N(10),
        N(0)|N(4)|N(7)|N(11), N(0)|N(3)|N(7)|N(10), N(0)|N(3)|N(6),
        N(0)|N(3)|N(6)|N(9), N(0)|N(3)|N(6)|N(10), N(0)|N(4)|N(8),
        N(0)|N(2)|N(7), N(0)|N(5)|N(7), N(0)|N(7), N(0)|N(4)|N(7)|N(9),
        N(0)|N(3)|N(7)|N(9), N(0)|N(2)|N(4)|N(7)
    };
    return masks[q];
    #undef N
}
