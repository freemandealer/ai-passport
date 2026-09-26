#include "frog_ui.h"
#include "frog_text.h"
#include "frog_glyphs.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(frog_font_14);
LV_FONT_DECLARE(frog_font_20);

enum { PAPER = 0xFAF5E4, INK = 0x254D38, MOSS = 0x668663,
       WATER = 0xD3E8CD, GREEN = 0x87BD4C, LIGHT = 0xC6EB82 };
static lv_obj_t *s_screen, *s_scene, *s_status, *s_score, *s_best;
static lv_obj_t *s_battery, *s_hint, *s_detail, *s_button;
static frog_model_t s_view;
static uint32_t s_now;

/* Original 20 x 16 sprite. Horizontal runs are draw commands, not dozens of
 * widgets or a full-screen framebuffer on the no-PSRAM board. */
static const char *const FROG[] = {
    "....000......000....",
    "...01110....01110...",
    "..0122210000122210..",
    "..0123321111233210..",
    "..0123321111233210..",
    "..0122211111222210..",
    ".011111111111111110.",
    "01141111111111114110",
    "01111111000111111110",
    ".011111100011111110.",
    "..0111222222211110..",
    "..0112222222221110..",
    ".011122222222211110.",
    "01110012222210011110",
    "0110..0000000..01110",
    ".00............000..",
};

static void rect(lv_layer_t *layer, int x, int y, int w, int h, uint32_t color, int radius) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color);
    d.bg_opa = LV_OPA_COVER;
    d.radius = radius;
    lv_area_t a = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(layer, &d, &a);
}

static void draw_scene(lv_event_t *e) {
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(s_scene, &coords);
    int ox = coords.x1, oy = coords.y1;
    rect(layer, ox + 14, oy + 124, 212, 132, WATER, 18);
    rect(layer, ox + 26, oy + 227, 188, 21, 0xB5D6BC, 10);
    rect(layer, ox + 74, oy + 242, 94, 9, MOSS, 4);
    rect(layer, ox + 78, oy + 240, 86, 7, GREEN, 3);
    rect(layer, ox + 119, oy + 240, 3, 5, WATER, 0);
    // Reeds, sparkles and ripples are original scene geometry.
    rect(layer, ox + 34, oy + 224, 3, 15, MOSS, 0);
    rect(layer, ox + 28, oy + 219, 3, 17, MOSS, 0);
    rect(layer, ox + 28, oy + 217, 3, 6, 0xD9AF65, 0);
    rect(layer, ox + 194, oy + 221, 3, 18, MOSS, 0);
    rect(layer, ox + 201, oy + 216, 3, 23, MOSS, 0);
    rect(layer, ox + 200, oy + 213, 5, 7, 0xD9AF65, 0);
    rect(layer, ox + 39, oy + 247, 22, 2, 0x8BB49C, 0);
    rect(layer, ox + 177, oy + 243, 16, 2, 0x8BB49C, 0);
    rect(layer, ox + 173, oy + 152, 10, 2, PAPER, 0);
    rect(layer, ox + 177, oy + 148, 2, 10, PAPER, 0);
    rect(layer, ox + 57, oy + 158, 4, 4, PAPER, 0);
    // Score-dependent motion always starts at this fixed ground coordinate.
    int fy = oy + 198 - frog_jump_offset(&s_view, s_now);
    if (s_view.phase == FROG_LISTENING) fy += (s_now / 250 % 2) * 2;
    if (s_view.phase == FROG_THINKING) fy += 3;
    const uint32_t colors[] = {INK, GREEN, LIGHT, PAPER, 0xEF9A7E};
    for (int y = 0; y < 16; ++y) {
        const char *row = FROG[y];
        for (int x = 0; row[x];) {
            char c = row[x];
            int end = x + 1;
            while (row[end] && row[end] == c) ++end;
            if (c != '.') {
                rect(layer, ox + 90 + x * 3, fy + y * 3,
                     (end - x) * 3, 3, colors[c - '0'], 0);
            }
            x = end;
        }
    }
    // Eye pupils and a smiling mouth use the same pixel grid.
    rect(layer, ox + 105, fy + 9, 3, 6, INK, 0);
    rect(layer, ox + 132, fy + 9, 3, 6, INK, 0);
    if (s_view.phase == FROG_LISTENING) {
        for (int i = 0; i < 7; ++i) {
            int h = 3 + (int)s_view.level * (i % 3 + 1) / 12;
            rect(layer, ox + 26 + i * 5, oy + 196 - h / 2, 3, h, MOSS, 1);
        }
    }
}

static lv_obj_t *label(int x, int y, int w, const lv_font_t *font, uint32_t color,
                       const char *text, lv_text_align_t align) {
    lv_obj_t *l = lv_label_create(s_screen);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, align, 0);
    lv_label_set_text(l, text);
    return l;
}

static void text(lv_obj_t *obj, const char *value) {
    if (strcmp(lv_label_get_text(obj), value)) lv_label_set_text(obj, value);
}

bool frog_ui_check_fonts(void) {
    const lv_font_t *fonts[] = {&frog_font_14, &frog_font_20};
    for (unsigned f = 0; f < 2; ++f) {
        for (unsigned i = 0; i < sizeof(FROG_GLYPHS) / sizeof(FROG_GLYPHS[0]); ++i) {
            lv_font_glyph_dsc_t d = {0};
            if (!lv_font_get_glyph_dsc(fonts[f], &d, FROG_GLYPHS[i], 0) || d.is_placeholder) {
                return false;
            }
        }
        lv_font_glyph_dsc_t missing = {0};
        if (lv_font_get_glyph_dsc(fonts[f], &missing, 0x9F98, 0) && !missing.is_placeholder) return false;
    }
    return true;
}

bool frog_ui_create(void) {
    if (!frog_ui_check_fonts()) return false;
    s_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    s_scene = lv_obj_create(s_screen);
    lv_obj_remove_style_all(s_scene);
    lv_obj_set_size(s_scene, 240, 320);
    lv_obj_remove_flag(s_scene, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_scene, draw_scene, LV_EVENT_DRAW_MAIN, NULL);
    label(20, 13, 138, &frog_font_20, INK, TXT_TITLE, LV_TEXT_ALIGN_LEFT);
    s_battery = label(164, 18, 53, &frog_font_14, MOSS, "--%", LV_TEXT_ALIGN_RIGHT);
    s_detail = label(20, 42, 200, &frog_font_14, MOSS, TXT_TAGLINE, LV_TEXT_ALIGN_LEFT);
    s_status = label(20, 70, 132, &frog_font_14, INK, TXT_READY, LV_TEXT_ALIGN_LEFT);
    s_best = label(20, 94, 116, &frog_font_14, MOSS, "", LV_TEXT_ALIGN_LEFT);
    s_score = label(144, 69, 78, &lv_font_montserrat_36, INK, "--", LV_TEXT_ALIGN_RIGHT);
    s_button = lv_obj_create(s_screen);
    lv_obj_set_pos(s_button, 18, 262);
    lv_obj_set_size(s_button, 204, 30);
    lv_obj_set_style_bg_color(s_button, lv_color_hex(INK), 0);
    lv_obj_set_style_border_width(s_button, 0, 0);
    lv_obj_set_style_radius(s_button, 10, 0);
    lv_obj_remove_flag(s_button, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    s_hint = label(22, 266, 196, &frog_font_14, PAPER, TXT_HOLD, LV_TEXT_ALIGN_CENTER);
    label(14, 299, 212, &frog_font_14, MOSS, TXT_FUN, LV_TEXT_ALIGN_CENTER);
    frog_model_init(&s_view);
    lv_screen_load(s_screen);
    return true;
}

void frog_ui_render(const frog_model_t *m, uint32_t now, int battery) {
    s_view = *m;
    s_now = now;
    const char *status = TXT_READY, *hint = TXT_HOLD, *detail = TXT_TAGLINE;
    switch (m->phase) {
        case FROG_LISTENING: status = TXT_LISTEN; hint = TXT_RELEASE; break;
        case FROG_THINKING: status = TXT_THINK; hint = TXT_WAIT; detail = "..."; break;
        case FROG_JUMPING:
        case FROG_RESULT:
            status = TXT_RESULT; hint = TXT_HOLD; detail = TXT_KEYS;
            if (m->phase == FROG_JUMPING) {
                detail = m->score == 100 ? TXT_FULL : m->score >= 80 ? TXT_HIGH :
                         m->score >= 40 ? TXT_MID : m->score ? TXT_LOW : TXT_ZERO;
            }
            break;
        case FROG_TOO_SHORT: status = TXT_SHORT; hint = TXT_RETRY; break;
        case FROG_TOO_QUIET: status = TXT_QUIET; hint = TXT_RETRY; break;
        case FROG_AUDIO_ERROR: status = TXT_AUDIO; hint = TXT_RETRY; break;
        case FROG_INPUT_ERROR: status = TXT_INPUT; hint = TXT_REBOOT; break;
        default: break;
    }
    char buf[64];
    if (m->held && m->phase != FROG_LISTENING && m->phase != FROG_INPUT_ERROR) hint = TXT_LIFT;
    text(s_status, status);
    text(s_hint, hint);
    if (m->phase == FROG_LISTENING) {
        unsigned elapsed = now - m->started_ms;
        snprintf(buf, sizeof(buf), "%u.%u / 10 s", elapsed / 1000, elapsed / 100 % 10);
        text(s_detail, buf);
    } else text(s_detail, detail);
    if (m->score < 0) text(s_score, "--");
    else { snprintf(buf, sizeof(buf), "%d", m->score); text(s_score, buf); }
    if (m->best < 0) snprintf(buf, sizeof(buf), TXT_BEST " -- " TXT_UNIT);
    else snprintf(buf, sizeof(buf), TXT_BEST " %d " TXT_UNIT, m->best);
    text(s_best, buf);
    if (battery < 0) text(s_battery, "--%");
    else { snprintf(buf, sizeof(buf), "%d%%", battery); text(s_battery, buf); }
    lv_obj_invalidate(s_scene);
}
