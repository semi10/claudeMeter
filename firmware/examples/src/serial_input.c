#include "serial_input.h"
#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"

static char buf[128];
static int buf_pos = 0;
bool serial_data_received = false;
uint32_t serial_last_rx_ms = 0;

static void parse_and_update(const char *json, int len, lvgl_data_struct *dat) {
    int sp = 0, wp = 0;
    char sr[32] = {0}, wr[32] = {0};

    const char *p;
    p = strstr(json, "\"sp\":");
    if (p) sscanf(p + 5, "%d", &sp);
    p = strstr(json, "\"wp\":");
    if (p) sscanf(p + 5, "%d", &wp);

    p = strstr(json, "\"sr\":\"");
    if (p) {
        p += 6;
        const char *e = strchr(p, '"');
        if (e) {
            int n = e - p;
            if (n < 32) { memcpy(sr, p, n); sr[n] = 0; }
        }
    }

    p = strstr(json, "\"wr\":\"");
    if (p) {
        p += 6;
        const char *e = strchr(p, '"');
        if (e) {
            int n = e - p;
            if (n < 32) { memcpy(wr, p, n); wr[n] = 0; }
        }
    }

    sp = sp < 0 ? 0 : sp > 100 ? 100 : sp;
    wp = wp < 0 ? 0 : wp > 100 ? 100 : wp;

    lv_arc_set_value(dat->arc_session, sp);
    lv_arc_set_value(dat->arc_weekly, wp);

    char tmp[8];
    snprintf(tmp, sizeof(tmp), "%d%%", sp);
    lv_label_set_text(dat->lbl_sp, tmp);
    snprintf(tmp, sizeof(tmp), "%d%%", wp);
    lv_label_set_text(dat->lbl_wp, tmp);

    lv_label_set_text(dat->lbl_sr, sr);
    lv_label_set_text(dat->lbl_wr, wr);
    lv_obj_clear_flag(dat->lbl_sr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(dat->lbl_wr, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(dat->lbl_status, LV_OBJ_FLAG_HIDDEN);

    /* Set arc colors based on percentage: green→yellow→red */
    lv_obj_set_style_arc_color(dat->arc_session, pct_to_color(sp), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(dat->arc_weekly, pct_to_color(wp), LV_PART_INDICATOR);

    serial_last_rx_ms = to_ms_since_boot(get_absolute_time());
    serial_data_received = true;
}

void serial_poll(lvgl_data_struct *dat) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (buf_pos >= 127) { buf_pos = 0; continue; }
        if (c == '\n') {
            buf[buf_pos] = '\0';
            parse_and_update(buf, buf_pos, dat);
            buf_pos = 0;
        } else {
            buf[buf_pos++] = (char)c;
        }
    }
}
