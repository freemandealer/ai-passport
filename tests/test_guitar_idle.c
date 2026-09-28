#include "guitar_idle.h"
#include "guitar_player.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static guitar_library_t lib;

int main(void)
{
    assert(guitar_parse(GUITAR_DEFAULT_SCORE, strlen(GUITAR_DEFAULT_SCORE), &lib, NULL));
    const uint64_t start = UINT64_C(9000000000), timeout = GUITAR_IDLE_TIMEOUT_US;
    const guitar_page_t pages[] = {G_SCORE_PAGE, G_SONG_PAGE, G_TEMPO_PAGE, G_VOLUME_PAGE, G_WIFI_PAGE};
    for (unsigned i = 0; i < sizeof(pages)/sizeof(pages[0]); ++i) {
        guitar_idle_t idle = {0};
        guitar_player_t p;
        guitar_player_init(&p, &lib); p.page = pages[i];
        assert(!guitar_idle_update(&idle, false, 0, start));
        assert(!guitar_idle_update(&idle, false, 0, start + timeout - 1));
        assert(guitar_idle_update(&idle, false, 0, start + timeout));
        /* An action at the boundary wins and grants another five minutes. */
        assert(!guitar_idle_update(&idle, false, 1, start + timeout));
        assert(!guitar_idle_update(&idle, false, 1, start + 2*timeout - 1));
        assert(guitar_idle_update(&idle, false, 1, start + 2*timeout));
    }
    for (unsigned mode = 0; mode < 2; ++mode) {
        guitar_idle_t idle = {0};
        guitar_player_t p = {.transport = mode ? G_PLAYING : G_COUNT_IN};
        bool playing = p.transport == G_PLAYING || p.transport == G_COUNT_IN;
        assert(!guitar_idle_update(&idle, playing, 0, start));
        assert(!guitar_idle_update(&idle, playing, 0, start + 100*timeout));
        assert(!guitar_idle_update(&idle, false, 0, start + 101*timeout));
        assert(!guitar_idle_update(&idle, false, 0, start + 102*timeout - 1));
        assert(guitar_idle_update(&idle, false, 0, start + 102*timeout));
    }
    guitar_idle_t idle = {0};
    assert(!guitar_idle_update(&idle, false, UINT32_MAX, start));
    assert(!guitar_idle_update(&idle, false, 0, start + timeout)); /* Activity wrap. */
    assert(!guitar_idle_update(&idle, false, 0, 1)); /* Defensive clock reset. */
    assert(guitar_idle_update(&idle, false, 0, timeout + 1));
    puts("Idle timer: PASS (all five pages, exact 5-minute boundary, activity, playback/count-in, pause/end, wrap)");
    return 0;
}
