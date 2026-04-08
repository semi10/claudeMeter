#ifndef _SERIAL_INPUT_H_
#define _SERIAL_INPUT_H_

#include "LVGL_example.h"

void serial_poll(lvgl_data_struct *dat);

/* Map percentage 0-100 to green→yellow→red */
static inline lv_color_t pct_to_color(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    uint8_t r, g;
    if (pct <= 50) {
        r = (uint8_t)(pct * 255 / 50);
        g = 255;
    } else {
        r = 255;
        g = (uint8_t)((100 - pct) * 255 / 50);
    }
    return lv_color_make(r, g, 0);
}

#endif
