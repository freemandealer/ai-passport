#include "key_ui.h"
#include "bsp_display_rounding.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t pixels[240 * 320];
static uint16_t draw_buffer[240 * 20];
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

static void snapshot(const char *dir, const char *name, key_session_t *s, int battery) {
    key_ui_render(s, battery, 3000);
    lv_obj_update_layout(lv_screen_active());
    check_labels(lv_screen_active());
    lv_refr_now(NULL);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
    FILE *file = fopen(path, "wb"); assert(file);
    fprintf(file, "P6\n240 320\n255\n");
    for (unsigned i=0; i<240*320; ++i) {
        uint16_t c=pixels[i];
        unsigned char rgb[]={(c>>11)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};
        assert(fwrite(rgb,1,3,file)==3);
    }
    fclose(file);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    lv_init();
    lv_display_t *display=lv_display_create(240,320);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,draw_buffer,NULL,sizeof(draw_buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    assert(key_ui_create() && key_ui_check_fonts());
    key_session_t s = {.first=-1,.second=-1};
    snapshot(argv[1],"ready",&s,88);
    s.mode=KEY_MUSIC; snapshot(argv[1],"ready-music",&s,88); s.mode=KEY_HUM;
    s.phase=KEY_LISTENING; s.frames=43; s.level=76;
    for (unsigned i=0; i<12; ++i) s.chroma[i]=(i*7)%13;
    snapshot(argv[1],"listening",&s,88);
    s.phase=KEY_RESULT; s.first=0; s.second=21; s.score=.86f;
    snapshot(argv[1],"result",&s,88);
    s.phase=KEY_AMBIGUOUS; s.first=21; s.second=0; s.score=.72f;
    snapshot(argv[1],"ambiguous",&s,-1);
    for (int phase=KEY_SHORT; phase<=KEY_ERROR; ++phase) {
        s.phase=phase;
        char name[32]; snprintf(name,sizeof(name),"error-%d",phase);
        snapshot(argv[1],name,&s,100);
    }
    for (int key=0; key<24; ++key) {
        s.phase=KEY_RESULT; s.first=key; s.second=(key+7)%24; s.score=1;
        s.limit_reached=key%2; s.mode=key%2;
        key_ui_render(&s,-1,0); lv_obj_update_layout(lv_screen_active());
        check_labels(lv_screen_active()); lv_refr_now(NULL);
    }
    s.phase=KEY_AMBIGUOUS; s.first=s.second=-1; s.limit_reached=false;
    snapshot(argv[1],"no-candidate",&s,-1);
    for (unsigned i=0; i<400; ++i) {
        s.phase=i%2 ? KEY_READY : KEY_LISTENING; s.level=i%101;
        key_ui_render(&s,88,i*100); lv_refr_now(NULL);
    }
    lv_mem_monitor_t monitor; lv_mem_monitor(&monitor);
    printf("LVGL UI: PASS (%u flushes; %u labels checked; 32KB pool, peak=%zu bytes)\n",frames,labels_checked,monitor.max_used);
    lv_deinit();
}
