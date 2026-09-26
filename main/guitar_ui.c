#include "guitar_ui.h"
#include "guitar_shapes.h"
#include <stdio.h>
#include <string.h>
#ifdef GUITAR_UI_TEST
#include <assert.h>
#endif

enum { BG = 0x070E0B, PANEL = 0x0D1C14, ACTIVE = 0x153824, GREEN = 0x88FFA8,
       INK = 0xDCF9DF, MUTED = 0x659B77, GRID = 0x365D42, AMBER = 0xF1C777 };

typedef struct {
    guitar_player_t player;
    guitar_network_info_t network;
    char title[GUITAR_TITLE_MAX], key[12], menu[5][GUITAR_TITLE_MAX];
    guitar_chord_t chords[3];
    char comments[3][GUITAR_COMMENT_MAX];
    bool present[3];
    uint16_t bar_count, song_count, menu_start;
    int battery;
} view_t;
static view_t s_view;
static lv_obj_t *s_screen;
static lv_obj_t *s_names[3];
static lv_font_t s_chord_font;

enum { SCORE_TOP = 55, SCORE_STEP = 85, SCORE_HEIGHT = 81 };

/* Text is already validated by the score parser or is a fixed application
 * string. Keep UTF-8 decoding independent of LVGL's private text helpers. */
static uint32_t next_codepoint(const char *value, uint32_t *index)
{
    const unsigned char *p = (const unsigned char *)value;
    uint32_t cp = p[(*index)++];
    if (cp < 0x80) return cp;
    unsigned extra = cp < 0xE0 ? 1 : cp < 0xF0 ? 2 : 3;
    cp &= (1u << (6 - extra)) - 1;
    while (extra--) cp = (cp << 6) | (p[(*index)++] & 63);
    return cp;
}

static void rect(lv_layer_t *l, int x, int y, int w, int h, uint32_t color, uint32_t border, int radius)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    d.radius = radius;
    d.border_width = border ? 1 : 0;
    d.border_color = lv_color_hex(border);
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(l, &d, &a);
}

static void line(lv_layer_t *l, int x1, int y1, int x2, int y2, uint32_t color, int width)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
    d.color = lv_color_hex(color); d.width = width;
    lv_draw_line(l, &d);
}

/* Measure and elide at UTF-8 boundaries. Rendering never wraps into a neighbour. */
static void text(lv_layer_t *l, int x, int y, int w, const char *value, const lv_font_t *font, uint32_t color)
{
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%s", value);
    lv_point_t size;
    lv_text_get_size(&size, buffer, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    if (size.x > w) {
        size_t n = strlen(buffer);
        do {
            do { --n; } while (n && ((unsigned char)buffer[n] & 0xC0) == 0x80);
            memcpy(buffer + n, "...", 4);
            lv_text_get_size(&size, buffer, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        } while (n && size.x > w);
    }
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text = buffer; d.text_local = 1; d.font = font; d.color = lv_color_hex(color);
    lv_area_t a = {x, y, x + w - 1, y + font->line_height - 1};
#ifdef GUITAR_UI_TEST
    assert(size.x <= w && x >= 0 && a.x2 < 240 && y >= 0 && a.y2 < 320);
    uint32_t i = 0;
    while (buffer[i]) {
        uint32_t cp = next_codepoint(buffer, &i);
        lv_font_glyph_dsc_t glyph = {0};
        assert(lv_font_get_glyph_dsc(font, &glyph, cp, 0) && !glyph.is_placeholder);
    }
#endif
    lv_draw_label(l, &d, &a);
}

static void cn(lv_layer_t *l, int x, int y, int w, const char *value, uint32_t color)
{ text(l, x, y, w, value, &guitar_font_16, color); }

static void shape(lv_layer_t *l, int y, guitar_chord_t chord, bool current)
{
    guitar_shape_t s = guitar_shape(chord);
    const int x = 148, top = y + 28, spacing = 8;
    uint32_t color = current ? GREEN : MUTED;
    char fret[8];
    snprintf(fret, sizeof(fret), "%u", s.first_fret);
    text(l, 131, top + 1, 16, fret, &lv_font_montserrat_12, MUTED);
    for (int i = 0; i < 6; ++i) {
        int sx = x + i * spacing;
        line(l, sx, top, sx, top + 45, GRID, 1);
        if (s.frets[i] <= 0) {
            text(l, sx - 3, top - 15, 9, s.frets[i] < 0 ? "x" : "o", &lv_font_montserrat_12, color);
        } else {
            int py = top + (s.frets[i] - s.first_fret) * 9 + 4;
            rect(l, sx - 3, py - 3, 7, 7, color, 0, 4);
        }
    }
    for (int f = 0; f <= 5; ++f) line(l, x, top + f * 9, x + 40, top + f * 9, GRID, f == 0 && s.first_fret == 1 ? 2 : 1);
}

static void battery(lv_layer_t *l)
{
    char value[12];
    if (s_view.battery < 0) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%d%%", s_view.battery);
    text(l, 192, 15, 33, value, &lv_font_montserrat_12, MUTED);
}

static void score(lv_layer_t *l)
{
    const guitar_player_t *p = &s_view.player;
    cn(l, 22, 12, 164, s_view.title, INK);
    char meta[48];
    const char *state = p->transport == G_PAUSED ? "PAUSE" : p->transport == G_FINISHED ? "END" : "PLAY";
    if (!s_view.network.storage_ok) state = "!SAVE";
    else if (s_view.network.audio_failed) state = "!AUDIO";
    else if (!p->volume) state = "MUTE";
    snprintf(meta, sizeof(meta), "%s / %u BPM / %u:%u  %s", s_view.key, p->bpm, p->bar + 1, s_view.bar_count, state);
    text(l, 22, 36, 202, meta, &lv_font_montserrat_12, GREEN);
    for (int i = 0; i < 3; ++i) {
        int y = SCORE_TOP + i * SCORE_STEP;
        rect(l, 12, y, 194, SCORE_HEIGHT, i == 0 ? ACTIVE : PANEL, i == 0 ? GREEN : GRID, 4);
        if (p->transport == G_COUNT_IN && i == 0) {
            text(l, 21, y + 25, 110, "COUNT-IN", &lv_font_montserrat_14, GREEN);
            snprintf(meta, sizeof(meta), "%u", p->eighth / 2 + 1);
            text(l, 157, y + 21, 32, meta, &lv_font_montserrat_28, GREEN);
        } else if (s_view.present[i]) {
            char degree[24];
            guitar_key_t k;
            guitar_parse_key(s_view.key, &k);
            guitar_degree_name(s_view.chords[i], k, degree, sizeof(degree));
            text(l, 22, y + 46, s_view.comments[i][0] ? 174 : 108, degree, &lv_font_montserrat_20, i == 0 ? GREEN : MUTED);
            if (!s_view.comments[i][0]) shape(l, y, s_view.chords[i], i == 0);
        } else text(l, 22, y + 30, 155, "END", &lv_font_montserrat_14, MUTED);
    }
    bool running = p->transport == G_COUNT_IN || p->transport == G_PLAYING;
    for (int beat = 0; beat < 4; ++beat) {
        bool lit = running && p->eighth / 2 == beat && p->eighth % 2 == 0;
        rect(l, 216, 58 + beat * 62, 12, 55, lit ? GREEN : PANEL,
             running && p->eighth / 2 == beat ? GREEN : GRID, 2);
    }
}

static void tempo(lv_layer_t *l)
{
    text(l, 24, 16, 150, "TEMPO", &lv_font_montserrat_14, GREEN);
    text(l, 25, 58, 190, "4/4", &lv_font_montserrat_14, MUTED);
    rect(l, 24, 95, 192, 100, ACTIVE, GREEN, 6);
    char value[16];
    snprintf(value, sizeof(value), "%u", s_view.player.candidate_bpm);
    text(l, 58, 115, 125, value, &lv_font_montserrat_48, INK);
    text(l, 93, 171, 60, "BPM", &lv_font_montserrat_14, GREEN);
    text(l, 67, 227, 134, "30 - 120 BPM", &lv_font_montserrat_14, MUTED);
}

static void volume(lv_layer_t *l)
{
    text(l, 24, 16, 150, "CLICK VOLUME", &lv_font_montserrat_14, GREEN);
    text(l, 25, 58, 190, "VOLUME / 0 - 10", &lv_font_montserrat_14, MUTED);
    rect(l, 24, 95, 192, 100, ACTIVE, GREEN, 6);
    char value[8];
    snprintf(value, sizeof(value), "%u", s_view.player.candidate_volume);
    text(l, 88, 116, 100, value, &lv_font_montserrat_48, INK);
    for (int i = 0; i < 10; ++i)
        rect(l, 25 + 19 * i, 218, 15, 26, i < s_view.player.candidate_volume ? GREEN : PANEL, GRID, 1);
    if (s_view.network.audio_failed)
        text(l, 54, 267, 162, "AUDIO OFFLINE", &lv_font_montserrat_14, AMBER);
    else if (!s_view.player.candidate_volume)
        text(l, 89, 267, 100, "MUTED", &lv_font_montserrat_14, MUTED);
}

static void songs(lv_layer_t *l)
{
    text(l, 24, 16, 155, "SELECT SONG", &lv_font_montserrat_14, GREEN);
    char count[32];
    snprintf(count, sizeof(count), "%u SONGS / SET LIST", s_view.song_count);
    text(l, 24, 46, 200, count, &lv_font_montserrat_12, MUTED);
    for (unsigned i = 0; i < 5; ++i) {
        unsigned index = s_view.menu_start + i;
        if (index > s_view.song_count) break;
        bool selected = index == s_view.player.selection;
        int y = 75 + i * 39;
        rect(l, 15, y, 210, 34, selected ? ACTIVE : PANEL, selected ? GREEN : GRID, 3);
        if (index == s_view.song_count) text(l, 24, y + 9, 190, "Wi-Fi score editor", &lv_font_montserrat_14, GREEN);
        else {
            snprintf(count, sizeof(count), "%02u", index + 1);
            text(l, 23, y + 9, 23, count, &lv_font_montserrat_12, MUTED);
            cn(l, 52, y + 7, 162, s_view.menu[i], selected ? INK : MUTED);
        }
    }
}

static void wifi(lv_layer_t *l)
{
    text(l, 24, 16, 163, "WI-FI EDITOR", &lv_font_montserrat_14, GREEN);
    text(l, 24, 59, 190, "CONNECT TO HOTSPOT", &lv_font_montserrat_14, INK);
    text(l, 24, 88, 199, s_view.network.ssid, &lv_font_montserrat_20, GREEN);
    text(l, 24, 130, 199, "NO PASSWORD NEEDED", &lv_font_montserrat_14, MUTED);
    text(l, 24, 193, 190, "OPEN IN BROWSER", &lv_font_montserrat_14, MUTED);
    text(l, 24, 220, 199, "192.168.4.1", &lv_font_montserrat_20, GREEN);
    text(l, 24, 254, 199, s_view.network.ready ? "EDIT & SAVE TO DEVICE" : "WI-FI UNAVAILABLE",
         &lv_font_montserrat_14, s_view.network.ready ? MUTED : AMBER);
}

static void draw(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    switch (s_view.player.page) {
        case G_SCORE_PAGE: score(layer); break;
        case G_TEMPO_PAGE: tempo(layer); break;
        case G_SONG_PAGE: songs(layer); break;
        case G_WIFI_PAGE: wifi(layer); break;
        case G_VOLUME_PAGE: volume(layer); break;
    }
    battery(layer);
}

bool guitar_ui_create(void)
{
    if (s_screen) return true;
    s_screen = lv_obj_create(NULL);
    if (!s_screen) return false;
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_screen, draw, LV_EVENT_DRAW_MAIN, NULL);
    s_chord_font = lv_font_montserrat_28;
    s_chord_font.fallback = &guitar_font_24;
    if (s_chord_font.line_height < guitar_font_24.line_height)
        s_chord_font.line_height = guitar_font_24.line_height;
    for (unsigned i = 0; i < 3; ++i) {
        s_names[i] = lv_label_create(s_screen);
        if (!s_names[i]) return false;
        lv_obj_remove_style_all(s_names[i]);
        lv_obj_set_style_text_font(s_names[i], &s_chord_font, 0);
        lv_obj_set_pos(s_names[i], 21, SCORE_TOP + i * SCORE_STEP + 8);
        lv_obj_set_height(s_names[i], 34);
        lv_obj_set_style_anim_duration(s_names[i], lv_anim_speed_clamped(35, 500, 10000), 0);
        lv_obj_add_flag(s_names[i], LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

void guitar_ui_render(const guitar_player_t *p, const guitar_library_t *lib,
                      int battery_value, const guitar_network_info_t *network)
{
    view_t next = {0};
    next.player = *p; next.battery = battery_value; next.network = *network;
    const guitar_song_t *song = &lib->songs[p->song];
    memcpy(next.title, song->title, sizeof(next.title));
    guitar_key_name(song->key, next.key, sizeof(next.key));
    next.bar_count = song->bar_count; next.song_count = lib->song_count;
    next.menu_start = (p->selection / 5) * 5;
    for (unsigned i = 0; i < 5 && next.menu_start + i < lib->song_count; ++i)
        memcpy(next.menu[i], lib->songs[next.menu_start + i].title, GUITAR_TITLE_MAX);
    for (int i = 0; i < 3; ++i) {
        int bar = p->bar + i - (p->transport == G_COUNT_IN ? 1 : 0);
        if (bar >= 0 && bar < song->bar_count) {
            next.present[i] = true;
            next.chords[i] = lib->bars[song->first_bar + bar];
            snprintf(next.comments[i], sizeof(next.comments[i]), "%s", guitar_chord_comment(lib, next.chords[i]));
        }
    }
    if (memcmp(&next, &s_view, sizeof(next))) {
        s_view = next;
        for (unsigned i = 0; i < 3; ++i) {
            bool visible = p->page == G_SCORE_PAGE && next.present[i] && !(p->transport == G_COUNT_IN && i == 0);
            lv_obj_set_flag(s_names[i], LV_OBJ_FLAG_HIDDEN, !visible);
            if (!visible) continue;
            char name[24], title[100];
            guitar_chord_name(next.chords[i], name, sizeof(name));
            snprintf(title, sizeof(title), "%s%s%s", name, next.comments[i][0] ? " " : "", next.comments[i]);
            lv_label_long_mode_t mode = next.comments[i][0] ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_DOT;
            if (lv_label_get_long_mode(s_names[i]) != mode) lv_label_set_long_mode(s_names[i], mode);
            lv_obj_set_width(s_names[i], next.comments[i][0] ? 174 : 108);
            lv_obj_set_style_text_color(s_names[i], lv_color_hex(i == 0 ? INK : MUTED), 0);
            /* Do not restart a long annotation's marquee on each half-beat. */
            if (strcmp(lv_label_get_text(s_names[i]), title)) lv_label_set_text(s_names[i], title);
        }
        lv_obj_invalidate(s_screen);
    }
    if (lv_screen_active() != s_screen) lv_screen_load(s_screen);
}

bool guitar_ui_check_fonts(void)
{
    /* Fixed labels are English; song titles and annotations retain CJK. */
    const char *fixed = "绿光练习小调漫步";
    uint32_t i = 0;
    while (fixed[i]) {
        uint32_t cp = next_codepoint(fixed, &i);
        lv_font_glyph_dsc_t glyph = {0};
        if (!lv_font_get_glyph_dsc(&guitar_font_16, &glyph, cp, 0) || glyph.is_placeholder) return false;
    }
    lv_font_glyph_dsc_t missing = {0};
    return !lv_font_get_glyph_dsc(&guitar_font_16, &missing, 0x1F3B8, 0) || missing.is_placeholder;
}
