#pragma once
#include "guitar_score.h"

typedef enum { G_PAUSED, G_COUNT_IN, G_PLAYING, G_FINISHED } guitar_transport_t;
typedef enum { G_SCORE_PAGE, G_TEMPO_PAGE, G_SONG_PAGE, G_WIFI_PAGE, G_VOLUME_PAGE } guitar_page_t;
typedef enum { G_UP, G_DOWN, G_OK, G_UP_DOUBLE, G_DOWN_DOUBLE, G_OK_LONG, G_UP_LONG, G_DOWN_LONG } guitar_action_t;
typedef struct {
    guitar_transport_t transport;
    guitar_page_t page;
    uint16_t song, bar, bpm, candidate_bpm, selection, start_bar;
    uint8_t eighth;
    uint8_t volume, candidate_volume;
    bool lead_in;
    uint64_t started_us;
} guitar_player_t;

void guitar_player_init(guitar_player_t *p, const guitar_library_t *lib);
void guitar_player_tick(guitar_player_t *p, const guitar_library_t *lib, uint64_t now_us);
void guitar_player_action(guitar_player_t *p, const guitar_library_t *lib, guitar_action_t action, uint64_t now_us);
void guitar_player_pause(guitar_player_t *p);
