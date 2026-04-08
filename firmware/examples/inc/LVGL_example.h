#ifndef _LVGL_EXAMPLE_H_
#define _LVGL_EXAMPLE_H_

#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "DEV_Config.h"
#include "lvgl.h"

typedef struct {
    lv_obj_t *scr;
    lv_obj_t *arc_session;
    lv_obj_t *arc_weekly;
    lv_obj_t *lbl_sp;     // session percentage
    lv_obj_t *lbl_sr;     // session reset time
    lv_obj_t *lbl_wp;     // weekly percentage
    lv_obj_t *lbl_wr;     // weekly reset time
    lv_obj_t *lbl_status; // "Waiting for data" / "Connection lost"
} lvgl_data_struct;

typedef void (*LCD_SetWindowsFunc)(uint16_t, uint16_t, uint16_t, uint16_t);

void LVGL_Init(void);

#endif
