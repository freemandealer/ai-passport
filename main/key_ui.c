#include "key_ui.h"
#include "key_text.h"
#include "key_glyphs.h"
#include <stdio.h>

LV_FONT_DECLARE(key_font_14);
LV_FONT_DECLARE(key_font_20);

#define BG 0x050D0A
#define PANEL 0x081C13
#define GREEN 0x83FFAC
#define MUTED 0x4B9B6B
#define LINE 0x204F37

static lv_obj_t *screen, *battery_label, *mode_label, *root_label, *kind_label;
static lv_obj_t *status_label, *detail_label, *time_label, *bars[12], *signal;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    if (!o) return NULL;
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label(int x, int y, int width, const lv_font_t *font, uint32_t color,
                       lv_text_align_t align, const char *text) {
    lv_obj_t *o = lv_label_create(screen);
    if (!o) return NULL;
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, width);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, align, 0);
    lv_label_set_text(o, text);
    return o;
}

bool key_ui_check_fonts(void) {
    const lv_font_t *fonts[] = {&key_font_14, &key_font_20};
    for (unsigned f = 0; f < 2; ++f) {
        for (unsigned i = 0; i < sizeof(KEY_GLYPHS) / sizeof(KEY_GLYPHS[0]); ++i) {
            lv_font_glyph_dsc_t d = {0};
            if (!lv_font_get_glyph_dsc(fonts[f], &d, KEY_GLYPHS[i], 0) || d.is_placeholder) return false;
        }
        lv_font_glyph_dsc_t absent = {0};
        if (lv_font_get_glyph_dsc(fonts[f], &absent, 0x9F98, 0) && !absent.is_placeholder) return false;
    }
    return true;
}

bool key_ui_create(void) {
    screen = box(NULL, 0, 0, 240, 320, BG);
    if (!screen) return false;
    label(24, 12, 140, &lv_font_montserrat_14, MUTED, LV_TEXT_ALIGN_LEFT, "SONIC / 01");
    battery_label = label(165, 11, 51, &key_font_14, GREEN, LV_TEXT_ALIGN_RIGHT, "--%");
    label(20, 34, 200, &key_font_20, GREEN, LV_TEXT_ALIGN_LEFT, TXT_TITLE);
    label(20, 61, 206, &lv_font_montserrat_14, MUTED, LV_TEXT_ALIGN_LEFT, "TONALITY ANALYZER");
    lv_obj_t *panel = box(screen, 16, 87, 208, 141, PANEL);
    if (!panel) return false;
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(LINE), 0);
    for (int diameter = 64; diameter <= 128; diameter += 32) {
        lv_obj_t *ring = box(screen, 120 - diameter / 2, 156 - diameter / 2, diameter, diameter, PANEL);
        if (!ring) return false;
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(LINE), 0);
    }
    box(screen, 28, 156, 184, 1, LINE);
    box(screen, 120, 100, 1, 112, LINE);
    mode_label = label(25, 92, 85, &key_font_14, MUTED, LV_TEXT_ALIGN_LEFT, TXT_HUM);
    time_label = label(146, 92, 68, &key_font_14, MUTED, LV_TEXT_ALIGN_RIGHT, "00.0 s");
    root_label = label(38, 113, 164, &lv_font_montserrat_48, GREEN, LV_TEXT_ALIGN_CENTER, "--");
    lv_obj_set_style_bg_color(root_label, lv_color_hex(PANEL), 0);
    lv_obj_set_style_bg_opa(root_label, LV_OPA_COVER, 0);
    kind_label = label(27, 169, 186, &key_font_20, GREEN, LV_TEXT_ALIGN_CENTER, TXT_READY);
    lv_obj_set_style_bg_color(kind_label, lv_color_hex(PANEL), 0);
    lv_obj_set_style_bg_opa(kind_label, LV_OPA_COVER, 0);
    signal = box(screen, 27, 213, 1, 3, GREEN);
    for (int i = 0; i < 12; ++i) bars[i] = box(screen, 22 + i * 17, 252, 9, 2, LINE);
    status_label = label(14, 261, 212, &key_font_14, GREEN, LV_TEXT_ALIGN_CENTER, TXT_HOLD);
    detail_label = label(14, 279, 212, &key_font_14, MUTED, LV_TEXT_ALIGN_CENTER, TXT_SUGGEST);
    label(20, 299, 200, &key_font_14, MUTED, LV_TEXT_ALIGN_CENTER, TXT_KEYS);
    if (!battery_label || !mode_label || !time_label || !root_label || !kind_label ||
        !signal || !status_label || !detail_label) return false;
    for (unsigned i = 0; i < 12; ++i) if (!bars[i]) return false;
    lv_screen_load(screen);
    return key_ui_check_fonts();
}

void key_ui_render(const key_session_t *s, int battery, uint32_t now_ms) {
    char text[96];
    if (battery < 0) lv_label_set_text(battery_label, "--%");
    else { snprintf(text, sizeof(text), "%d%%", battery); lv_label_set_text(battery_label, text); }
    lv_label_set_text(mode_label, s->mode == KEY_MUSIC ? TXT_MUSIC " / 02" : TXT_HUM " / 01");
    unsigned ms = s->frames * PITCH_FRAME_MS;
    snprintf(text, sizeof(text), "%02u.%u s", ms / 1000, ms / 100 % 10);
    lv_label_set_text(time_label, text);
    const char *kind = TXT_READY, *status = TXT_HOLD, *detail = TXT_SUGGEST;
    const char *root = "--";
    switch (s->phase) {
    case KEY_READY: detail = s->mode == KEY_MUSIC ? TXT_MUSIC_HELP : TXT_HUM_HELP; break;
    case KEY_LISTENING: kind = TXT_LISTEN; status = TXT_RELEASE; break;
    case KEY_RESULT:
    case KEY_AMBIGUOUS:
        root = pitch_note_name(s->first < 0 ? -1 : s->first % 12);
        kind = s->first < 0 ? TXT_UNCERTAIN : (s->first >= 12 ? TXT_MINOR : TXT_MAJOR);
        if (s->phase == KEY_AMBIGUOUS && s->first >= 0)
            kind = s->first >= 12 ? TXT_CANDIDATE_MINOR : TXT_CANDIDATE_MAJOR;
        if (s->first >= 0) {
            snprintf(text, sizeof(text), TXT_MATCH " %u%%  /  " TXT_ALTERNATIVE " %s%s",
                     (unsigned)(s->score > 0 ? s->score * 100 : 0),
                     pitch_note_name(s->second < 0 ? -1 : s->second % 12),
                     s->second >= 12 ? TXT_MINOR : TXT_MAJOR);
            status = text;
        } else status = TXT_UNCERTAIN;
        detail = s->phase == KEY_AMBIGUOUS
            ? (s->score < 0.60f ? TXT_LOW_MATCH : TXT_NOT_UNIQUE) : TXT_REFERENCE;
        break;
    case KEY_SHORT: kind = TXT_SHORT; status = TXT_RETRY_SHORT; break;
    case KEY_QUIET: kind = TXT_QUIET; status = TXT_RETRY_QUIET; break;
    case KEY_FEW_NOTES: kind = TXT_FEW; status = TXT_RETRY_FEW; break;
    case KEY_CLIPPED: kind = TXT_CLIP; status = TXT_RETRY_CLIP; break;
    case KEY_ERROR: kind = TXT_ERROR; status = TXT_RETRY_ERROR; break;
    default: break;
    }
    if (s->limit_reached) detail = TXT_LIMIT;
    lv_label_set_text(root_label, root);
    lv_label_set_text(kind_label, kind);
    lv_label_set_text(status_label, status);
    lv_label_set_text(detail_label, detail);
    lv_obj_set_width(signal, s->phase == KEY_LISTENING ? 1 + s->level * 185 / 100 : 186);
    lv_obj_set_style_bg_color(signal, lv_color_hex(s->phase == KEY_LISTENING ? GREEN : LINE), 0);
    float max = 1;
    for (unsigned i = 0; i < 12; ++i) if (s->chroma[i] > max) max = s->chroma[i];
    for (unsigned i = 0; i < 12; ++i) {
        unsigned height = 2 + (unsigned)(s->chroma[i] * 20 / max);
        lv_obj_set_height(bars[i], height);
        lv_obj_set_y(bars[i], 254 - height);
        bool active = s->chroma[i] > 0 || (s->phase == KEY_LISTENING && (now_ms / 128) % 12 == i);
        lv_obj_set_style_bg_color(bars[i], lv_color_hex(active ? GREEN : LINE), 0);
    }
}
