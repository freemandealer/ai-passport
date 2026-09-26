#pragma once
#include "guitar_score.h"

typedef struct {
    int8_t frets[6]; /* Low E to high e; -1 muted, 0 open, positive fret. */
    uint8_t first_fret;
} guitar_shape_t;

guitar_shape_t guitar_shape(guitar_chord_t chord);
uint16_t guitar_chord_intervals(guitar_quality_t quality);
