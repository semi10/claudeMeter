#ifndef _APP_H_
#define _APP_H_

#include "pico/stdlib.h"
#include "DEV_Config.h"
#include "LVGL_example.h"

extern LCD_SetWindowsFunc LCD_SetWindows;
extern uint16_t DISP_HOR_RES;
extern uint16_t DISP_VER_RES;

int LCD_1in14_test(void);

#endif
