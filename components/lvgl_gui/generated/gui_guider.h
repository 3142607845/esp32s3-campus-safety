/*
* Copyright 2024 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef GUI_GUIDER_H
#define GUI_GUIDER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

typedef struct
{
  
	lv_obj_t *counter;
	bool counter_del; // @suppress("Type cannot be resolved")
	lv_obj_t *counter_label_gpio;
	lv_obj_t *counter_label_tf_rw;
	lv_obj_t *counter_label_tf_state;
	lv_obj_t *counter_label_eth_ip;
	lv_obj_t *counter_label_eth_state;
	lv_obj_t *counter_label_wifi_ip;
	lv_obj_t *counter_label_wifi_state;
}lv_ui;

void ui_init_style(lv_style_t * style);
void init_scr_del_flag(lv_ui *ui);
void setup_ui(lv_ui *ui);
extern lv_ui guider_ui;

void setup_scr_counter(lv_ui *ui);

LV_FONT_DECLARE(lv_font_simsun_16)
LV_FONT_DECLARE(lv_font_arial_10)


#ifdef __cplusplus
}
#endif
#endif
