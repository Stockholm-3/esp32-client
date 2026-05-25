#ifndef UI_TAB_ELPRIS_H
#define UI_TAB_ELPRIS_H

#include "lvgl.h"
#include "elpris_api.h"

// External UI object declarations
extern lv_obj_t* ui_panel_price_header;
extern lv_obj_t* ui_lbl_price_now;
extern lv_obj_t* ui_lbl_price_val;
extern lv_obj_t* ui_lbl_price_unit;
extern lv_obj_t* ui_lbl_price_hi;
extern lv_obj_t* ui_lbl_price_lo;
extern lv_obj_t* ui_lbl_price_avg;
extern lv_obj_t* ui_panel_chart;
extern lv_obj_t* ui_chart_elpris;
extern lv_obj_t* ui_chart_elpris_Xaxis;
extern lv_obj_t* ui_chart_elpris_Yaxis1;
extern lv_obj_t* ui_lbl_leg_cheap;
extern lv_obj_t* ui_lbl_leg_avg;
extern lv_obj_t* ui_lbl_leg_exp;
extern lv_obj_t* ui_lbl_leg_now;

// Global chart data array
extern lv_coord_t g_elpris_data[24];

// Function declarations
void ui_tab_elpris_init(void);
void ui_elpris_update_display(const ElprisData* data);
void ui_elpris_set_current_hour(uint8_t hour);

#endif // UI_TAB_ELPRIS_H