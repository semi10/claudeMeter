/*****************************************************************************
* | File      	:   LCD_1in14_LVGL_test.c
* | Author      :   Waveshare team
* | Function    :   1.14inch LCD LVGL demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-01-07
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "LCD_Test.h"
#include "LCD_1in14.h"
#include "serial_input.h"

static void Widgets_Init(lvgl_data_struct *dat);

extern bool serial_data_received;
extern uint32_t serial_last_rx_ms;

/********************************************************************************
function:   Create a single arc gauge panel
parameter:  parent - parent object, y_offset - vertical position
            arc_out - pointer to store arc, pct_out - percentage label,
            reset_out - reset time label, title - panel title string
********************************************************************************/
static void create_gauge_panel(lv_obj_t *parent, lv_coord_t y_offset,
                               lv_obj_t **arc_out, lv_obj_t **pct_out,
                               lv_obj_t **reset_out, const char *title)
{
    /* Container panel - 135x120 */
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 135, 120);
    lv_obj_set_pos(panel, 0, y_offset);
    lv_obj_set_style_bg_color(panel, lv_color_black(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label at top */
    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 2);

    /* Arc gauge */
    lv_obj_t *arc = lv_arc_create(panel);
    lv_obj_set_size(arc, 90, 90);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_make(40, 40, 40), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(0, 180, 255), LV_PART_INDICATOR);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 8);
    *arc_out = arc;

    /* Percentage label centered on arc */
    lv_obj_t *lbl_pct = lv_label_create(panel);
    lv_label_set_text(lbl_pct, "--");
    lv_obj_set_style_text_color(lbl_pct, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_pct, &lv_font_montserrat_16, 0);
    lv_obj_align_to(lbl_pct, arc, LV_ALIGN_CENTER, 0, 0);
    *pct_out = lbl_pct;

    /* Reset time label below arc */
    lv_obj_t *lbl_reset = lv_label_create(panel);
    lv_label_set_text(lbl_reset, "");
    lv_obj_set_style_text_color(lbl_reset, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_reset, LV_ALIGN_BOTTOM_MID, 0, -2);
    *reset_out = lbl_reset;
}

/********************************************************************************
function:   Main function
parameter:
********************************************************************************/
int LCD_1in14_test(void)
{
    if(DEV_Module_Init()!=0){
        return -1;
    }

    /* LCD Init - portrait mode */
    printf("ClaudeMeter starting...\r\n");
    LCD_1IN14_Init(VERTICAL);
    LCD_1IN14_Clear(BLACK);

    /*Config parameters - portrait: 135 wide x 240 tall*/
    LCD_SetWindows = LCD_1IN14_SetWindows;
    DISP_HOR_RES = 135;
    DISP_VER_RES = 240;

    /*Init LVGL data structure*/
    lvgl_data_struct *dat = (lvgl_data_struct *)malloc(sizeof(lvgl_data_struct));
    memset(dat, 0, sizeof(lvgl_data_struct));

    /*Init LVGL*/
    LVGL_Init();
    Widgets_Init(dat);

    while(1)
    {
        lv_task_handler();
        serial_poll(dat);

        /* 2min timeout: show "Connection lost" if no data received recently */
        if (serial_data_received &&
            (to_ms_since_boot(get_absolute_time()) - serial_last_rx_ms > 120000)) {
            lv_label_set_text(dat->lbl_status, "Connection lost");
            lv_obj_clear_flag(dat->lbl_status, LV_OBJ_FLAG_HIDDEN);
        }

        DEV_Delay_ms(5);
    }

    DEV_Module_Exit();
    return 0;
}

/********************************************************************************
function:   Initialize Widgets - ClaudeMeter dual gauge UI
parameter:
********************************************************************************/
static void Widgets_Init(lvgl_data_struct *dat)
{
    /* Single screen with black background */
    dat->scr[0] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(dat->scr[0], lv_color_black(), 0);

    /* Top panel: Session gauge (y=0) */
    create_gauge_panel(dat->scr[0], 0,
                       &dat->arc_session, &dat->lbl_sp, &dat->lbl_sr, "Session");

    /* Bottom panel: Weekly gauge (y=120) */
    create_gauge_panel(dat->scr[0], 120,
                       &dat->arc_weekly, &dat->lbl_wp, &dat->lbl_wr, "Weekly");

    /* Status overlay label - centered on screen */
    dat->lbl_status = lv_label_create(dat->scr[0]);
    lv_label_set_text(dat->lbl_status, "Waiting for data");
    lv_obj_set_style_text_color(dat->lbl_status, lv_color_make(200, 200, 200), 0);
    lv_obj_set_style_text_font(dat->lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(dat->lbl_status, LV_ALIGN_CENTER, 0, 0);

    /* Load screen */
    lv_scr_load(dat->scr[0]);
}
