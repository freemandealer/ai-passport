#include "guitar_ui.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t pixels[240 * 320], buffer[240 * 20];
static unsigned frames;
static int battery_percent = 88;
static guitar_library_t lib;
static guitar_network_info_t network = {.ssid="Guitar-DEMO", .ready=true, .storage_ok=true};

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data)
{
    const uint16_t *src = (const uint16_t *)data;
    for (int y = area->y1; y <= area->y2; ++y) {
        int32_t left, right;
        assert(bsp_display_rounded_row_span(y, 240, 320, 30, &left, &right));
        for (int x = area->x1; x <= area->x2; ++x) {
            uint16_t value = *src++;
            pixels[y * 240 + x] = x >= left && x <= right ? value : 0;
        }
    }
    ++frames;
    lv_display_flush_ready(display);
}

static void snapshot(const char *directory, const char *name, guitar_player_t *p)
{
    guitar_ui_render(p, &lib, battery_percent, &network);
    lv_refr_now(NULL);
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.ppm", directory, name);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < 240 * 320; ++i) {
        uint16_t c = pixels[i];
        const unsigned char rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    fclose(file);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(guitar_ui_create() && guitar_ui_check_fonts());
    /* Exhaustive dynamic-title coverage through the exact font used to draw. */
    for (uint32_t cp = 0x4E00; cp <= GUITAR_CJK_LAST; ++cp) {
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&guitar_font_16, &glyph, cp, 0) || glyph.is_placeholder)
            fprintf(stderr, "Missing U+%04X\n", cp);
        assert(lv_font_get_glyph_dsc(&guitar_font_16, &glyph, cp, 0) && !glyph.is_placeholder);
    }
    for (uint32_t cp = 32; cp < 127; ++cp) {
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(&guitar_font_16, &glyph, cp, 0) && !glyph.is_placeholder);
    }
    const uint32_t punctuation[] = {0x3002, 0x300A, 0x300B, 0xFF08, 0xFF09, 0xFF0C, 0xFF01, 0xFF1F};
    for (unsigned i = 0; i < sizeof(punctuation)/sizeof(punctuation[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(&guitar_font_16, &glyph, punctuation[i], 0) && !glyph.is_placeholder);
    }
    assert(guitar_parse(GUITAR_DEFAULT_SCORE, strlen(GUITAR_DEFAULT_SCORE), &lib, NULL));
    guitar_player_t p;
    guitar_player_init(&p, &lib);
    snapshot(argv[1], "score", &p);
    lv_point_t battery_size;
    lv_text_get_size(&battery_size, "100%", &lv_font_montserrat_12, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    assert(battery_size.x <= 33); /* Even a full charge must not be elided. */
    battery_percent = -1; snapshot(argv[1], "battery-unavailable", &p);
    battery_percent = 0; snapshot(argv[1], "battery-empty", &p);
    battery_percent = 100; snapshot(argv[1], "battery-full", &p);
    battery_percent = 88;
    guitar_player_action(&p, &lib, G_OK, 0);
    snapshot(argv[1], "count-in", &p);
    guitar_player_tick(&p, &lib, 3000000);
    snapshot(argv[1], "playing", &p);
    guitar_player_action(&p, &lib, G_DOWN_LONG, 3000001);
    snapshot(argv[1], "tempo", &p);
    guitar_player_action(&p, &lib, G_UP_LONG, 3000001);
    snapshot(argv[1], "volume", &p);
    p.candidate_volume = 0;
    snapshot(argv[1], "muted", &p);
    guitar_player_action(&p, &lib, G_OK_LONG, 3000002);
    snapshot(argv[1], "songs", &p);
    p.selection = lib.song_count;
    guitar_player_action(&p, &lib, G_OK, 3000003);
    snapshot(argv[1], "wifi", &p);
    p.page = G_SCORE_PAGE; p.transport = G_PLAYING;
    /* Exercise every voicing, end-of-song, long names, missing battery, 400
     * redraws and page transitions under the same 24 KiB LVGL pool. */
    strcpy(lib.songs[0].title, "一二三四五六七八九十一二三四五六七八九十一二三四");
    for (unsigned i = 0; i < 400; ++i) {
        p.eighth = i % 8;
        p.bar = i % lib.songs[0].bar_count;
        for (unsigned j = 0; j < lib.bar_count; ++j)
            lib.bars[j] = (guitar_chord_t){.pitch = (i + j) % 12, .letter = (i + j) % 7, .quality = (i + j) % G_QUALITY_COUNT};
        guitar_ui_render(&p, &lib, -1, &network);
        lv_refr_now(NULL);
    }
    p.transport = G_FINISHED;
    snapshot(argv[1], "end", &p);
    const char *annotated = "---\n注释练习\nC\n80\nC(轻扫) 6(渐强 分解节奏) G(最后一拍停)";
    assert(guitar_parse(annotated, strlen(annotated), &lib, NULL));
    guitar_player_init(&p, &lib);
    snapshot(argv[1], "comments", &p);
    unsigned labels = 0;
    for (unsigned i = 0; i < lv_obj_get_child_count(lv_screen_active()); ++i) {
        lv_obj_t *label = lv_obj_get_child(lv_screen_active(), i);
        if (!lv_obj_check_type(label, &lv_label_class) || lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN)) continue;
        const lv_font_t *font = lv_obj_get_style_text_font(label, 0);
        assert(font->fallback == &guitar_font_24);
        lv_area_t area; lv_obj_get_coords(label, &area);
        assert(area.x1 >= 0 && area.x2 < 240 && area.y1 >= 0 && area.y2 < 320);
        if (!labels) assert(!strcmp(lv_label_get_text(label), "C 轻扫"));
        ++labels;
    }
    assert(labels == 3);
    for (uint32_t cp = 0x4E00; cp <= GUITAR_CJK_LAST; ++cp) {
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(&guitar_font_24, &glyph, cp, 0) && !glyph.is_placeholder);
    }
    for (uint32_t cp = 32; cp < 127; ++cp) {
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(&guitar_font_24, &glyph, cp, 0) && !glyph.is_placeholder);
    }
    for (unsigned i = 0; i < sizeof(punctuation)/sizeof(punctuation[0]); ++i) {
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(&guitar_font_24, &glyph, punctuation[i], 0) && !glyph.is_placeholder);
    }
    lv_font_glyph_dsc_t missing = {0};
    assert(!lv_font_get_glyph_dsc(&guitar_font_24, &missing, 0x1F3B8, 0) || missing.is_placeholder);
    /* Marquee continues across visual metronome updates without resetting. */
    uint16_t initial_text[34][174];
    for (unsigned y = 0; y < 34; ++y)
        memcpy(initial_text[y], &pixels[(148 + y) * 240 + 21], sizeof(initial_text[y]));
    bool scrolled = false;
    for (unsigned i = 0; i < 400; ++i) {
        p.eighth = i % 8;
        guitar_ui_render(&p, &lib, 100, &network);
        lv_tick_inc(20); lv_timer_handler(); lv_refr_now(NULL);
        for (unsigned y = 0; y < 34; ++y)
            if (memcmp(initial_text[y], &pixels[(148 + y) * 240 + 21], sizeof(initial_text[y]))) scrolled = true;
    }
    assert(scrolled);
    lv_mem_monitor_t monitor;
    lv_mem_monitor(&monitor);
    printf("LVGL: PASS, 20976 CJK + ASCII + punctuation at 16/24 px, marquee moves during updates, %u flushes, 24 KiB pool peak %zu bytes\n", frames, monitor.max_used);
    lv_deinit();
    return 0;
}
