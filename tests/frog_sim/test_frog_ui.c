#include "frog_ui.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t pixels[240 * 320];
static uint16_t draw_buffer[240 * 40];
static unsigned frames, labels_checked;

static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
    uint16_t *src = (uint16_t *)data;
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

static void check_labels(lv_obj_t *obj) {
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char *str = lv_label_get_text(obj);
        const lv_font_t *font = lv_obj_get_style_text_font(obj, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        assert(size.x <= lv_obj_get_width(obj));
        lv_area_t area;
        lv_obj_get_coords(obj, &area);
        assert(area.x1 >= 0 && area.x2 < 240 && area.y1 >= 0 && area.y2 < 320);
        const unsigned char *p = (const unsigned char *)str;
        while (*p) {
            uint32_t cp = *p++;
            if (cp >= 0xC0) {
                int extra = cp < 0xE0 ? 1 : cp < 0xF0 ? 2 : 3;
                cp &= (1u << (6 - extra)) - 1;
                while (extra--) { assert((*p & 0xC0) == 0x80); cp = (cp << 6) | (*p++ & 63); }
            }
            lv_font_glyph_dsc_t d = {0};
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        ++labels_checked;
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(obj); ++i) check_labels(lv_obj_get_child(obj, i));
}

static void snapshot(const char *dir, const char *name, frog_model_t *m, uint32_t now, int battery) {
    frog_ui_render(m, now, battery);
    lv_obj_update_layout(lv_screen_active());
    check_labels(lv_screen_active());
    lv_refr_now(NULL);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *file = fopen(path, "wb");
    assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i = 0; i < 240 * 320; ++i) {
        uint16_t c = pixels[i];
        unsigned char rgb[] = {(c >> 11) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31};
        assert(fwrite(rgb, 1, 3, file) == 3);
    }
    fclose(file);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    lv_init();
    lv_display_t *display = lv_display_create(240, 320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush);
    assert(frog_ui_create() && frog_ui_check_fonts());
    frog_model_t m;
    frog_model_init(&m);
    snapshot(argv[1], "ready", &m, 0, 88);
    m.phase = FROG_LISTENING; m.level = 80;
    snapshot(argv[1], "listening", &m, 2300, 88);
    m.phase = FROG_THINKING;
    snapshot(argv[1], "thinking", &m, 3000, -1);
    const int scores[] = {0, 50, 100};
    for (unsigned i = 0; i < 3; ++i) {
        m.score = m.best = scores[i]; m.phase = FROG_JUMPING; m.phase_ms = 0;
        char name[32]; snprintf(name, sizeof(name), "jump-%d", scores[i]);
        snapshot(argv[1], name, &m, FROG_JUMP_MS / 2, 100);
    }
    m.phase = FROG_RESULT;
    snapshot(argv[1], "result", &m, 4000, 88);
    for (int phase = FROG_TOO_SHORT; phase <= FROG_INPUT_ERROR; ++phase) {
        m.phase = phase; m.score = -1;
        char name[32]; snprintf(name, sizeof(name), "error-%d", phase);
        snapshot(argv[1], name, &m, 5000, -1);
    }
    // Render complete trajectories repeatedly through the real LVGL renderer.
    for (unsigned round = 0; round < 20; ++round) {
        m.phase = FROG_JUMPING; m.phase_ms = 0; m.score = round * 5;
        for (unsigned t = 0; t <= FROG_JUMP_MS; t += 40) {
            frog_ui_render(&m, t, 88);
            lv_refr_now(NULL);
        }
    }
    printf("LVGL 9.5 UI: PASS (%u flushes; %u labels, glyphs and bounds checked)\n", frames, labels_checked);
    lv_deinit();
}
