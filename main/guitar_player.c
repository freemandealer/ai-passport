#include "guitar_player.h"

void guitar_player_init(guitar_player_t *p, const guitar_library_t *lib)
{
    *p = (guitar_player_t){.transport = G_PAUSED, .bpm = lib->songs[0].bpm, .volume = 5};
}

void guitar_player_pause(guitar_player_t *p)
{
    p->transport = G_PAUSED;
    p->eighth = 0;
}

void guitar_player_tick(guitar_player_t *p, const guitar_library_t *lib, uint64_t now)
{
    if (p->transport != G_COUNT_IN && p->transport != G_PLAYING) return;
    if (now < p->started_us) return;
    /* Absolute rational time prevents accumulated truncation drift at 85/95 BPM.
     * A song is bounded to 512 bars; keep arithmetic bounded after long sleeps. */
    uint64_t elapsed = now - p->started_us;
    if (elapsed > UINT64_C(86400000000)) elapsed = UINT64_C(86400000000);
    uint64_t step = elapsed * p->bpm / UINT64_C(30000000);
    p->eighth = step % 8;
    unsigned intro = p->lead_in ? 8 : 0;
    if (step < intro) { p->transport = G_COUNT_IN; return; }
    uint64_t bar = p->start_bar + (step - intro) / 8;
    if (bar >= lib->songs[p->song].bar_count) {
        p->bar = lib->songs[p->song].bar_count - 1;
        p->transport = G_FINISHED;
        p->eighth = 0;
    } else {
        p->bar = (uint16_t)bar;
        p->transport = G_PLAYING;
    }
}

void guitar_player_action(guitar_player_t *p, const guitar_library_t *lib, guitar_action_t a, uint64_t now)
{
    guitar_player_tick(p, lib, now);
    if (a == G_OK_LONG) {
        guitar_player_pause(p);
        p->page = G_SONG_PAGE;
        p->selection = p->song;
        return;
    }
    if (a == G_DOWN_LONG || a == G_UP_LONG) {
        guitar_player_pause(p);
        p->page = a == G_DOWN_LONG ? G_TEMPO_PAGE : G_VOLUME_PAGE;
        p->candidate_bpm = p->bpm;
        p->candidate_volume = p->volume;
        return;
    }
    if (p->page == G_WIFI_PAGE) {
        if (a == G_OK) p->page = G_SCORE_PAGE;
        return;
    }
    if (p->page == G_SONG_PAGE) {
        int move = a == G_UP ? -1 : a == G_DOWN ? 1 : a == G_UP_DOUBLE ? -2 : a == G_DOWN_DOUBLE ? 2 : 0;
        p->selection = (p->selection + lib->song_count + 1 + move) % (lib->song_count + 1);
        if (a == G_OK) {
            if (p->selection == lib->song_count) p->page = G_WIFI_PAGE;
            else {
                p->song = p->selection;
                p->bar = 0;
                p->bpm = lib->songs[p->song].bpm;
                p->page = G_SCORE_PAGE;
                guitar_player_pause(p);
            }
        }
        return;
    }
    if (p->page == G_TEMPO_PAGE) {
        int move = a == G_UP ? 1 : a == G_DOWN ? -1 : a == G_UP_DOUBLE ? 2 : a == G_DOWN_DOUBLE ? -2 : 0;
        int candidate = p->candidate_bpm + move;
        p->candidate_bpm = candidate < 30 ? 30 : candidate > 120 ? 120 : candidate;
        if (a == G_OK) { p->bpm = p->candidate_bpm; p->page = G_SCORE_PAGE; }
        return;
    }
    if (p->page == G_VOLUME_PAGE) {
        int move = a == G_UP ? 1 : a == G_DOWN ? -1 : a == G_UP_DOUBLE ? 2 : a == G_DOWN_DOUBLE ? -2 : 0;
        int candidate = p->candidate_volume + move;
        p->candidate_volume = candidate < 0 ? 0 : candidate > 10 ? 10 : candidate;
        if (a == G_OK) { p->volume = p->candidate_volume; p->page = G_SCORE_PAGE; }
        return;
    }
    if (a == G_OK) {
        if (p->transport == G_PLAYING || p->transport == G_COUNT_IN) guitar_player_pause(p);
        else {
            if (p->transport == G_FINISHED) p->bar = 0;
            p->transport = G_COUNT_IN;
            p->started_us = now;
            p->start_bar = p->bar;
            p->eighth = 0;
            p->lead_in = true;
        }
    } else if (a == G_UP || a == G_DOWN || a == G_UP_DOUBLE || a == G_DOWN_DOUBLE) {
        int bar = a == G_UP_DOUBLE ? 0 : p->bar + (a == G_UP ? -1 : a == G_DOWN_DOUBLE ? 2 : 1);
        if (bar < 0) bar = 0;
        if (bar >= lib->songs[p->song].bar_count) bar = lib->songs[p->song].bar_count - 1;
        p->bar = bar;
        if (p->transport == G_PLAYING || p->transport == G_COUNT_IN) {
            /* Live navigation restarts the selected bar. During count-in it
             * restarts the four-beat lead-in for the newly selected bar. */
            p->lead_in = p->transport == G_COUNT_IN;
            p->start_bar = p->bar;
            p->started_us = now;
            p->eighth = 0;
        } else guitar_player_pause(p);
    }
}
