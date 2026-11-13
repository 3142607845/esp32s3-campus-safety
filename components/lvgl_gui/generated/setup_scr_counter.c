/*
* Copyright 2024 NXP
* NXP Confidential and Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#include "lvgl.h"
#include <stdio.h>
#include "gui_guider.h"
#include "events_init.h"
#include "widgets_init.h"
#include "custom.h"


void setup_scr_counter(lv_ui *ui)
{
	//Write codes counter
	ui->counter = lv_obj_create(NULL);
	lv_obj_set_size(ui->counter, 128, 160);

	//Write style for counter, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_bg_opa(ui->counter, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(ui->counter, lv_color_hex(0xffffff), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_grad_dir(ui->counter, LV_GRAD_DIR_VER, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_grad_color(ui->counter, lv_color_hex(0x4e95cb), LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_gpio
	ui->counter_label_gpio = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_gpio, "Smoke:--");
	lv_label_set_long_mode(ui->counter_label_gpio, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_gpio, 0, 135);
	lv_obj_set_size(ui->counter_label_gpio, 128, 16);

	//Write style for counter_label_gpio, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_gpio, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_gpio, lv_color_hex(0x024f5a), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_gpio, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_gpio, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_gpio, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_gpio, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(ui->counter_label_gpio, lv_color_hex(0x00ff00), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_gpio, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_tf_rw
	ui->counter_label_tf_rw = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_tf_rw, "Humi:--");
	lv_label_set_long_mode(ui->counter_label_tf_rw, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_tf_rw, 0, 108);
	lv_obj_set_size(ui->counter_label_tf_rw, 128, 16);

	//Write style for counter_label_tf_rw, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_tf_rw, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_tf_rw, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_tf_rw, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_tf_rw, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_tf_rw, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_tf_state
	ui->counter_label_tf_state = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_tf_state, "Temp:--");
	lv_label_set_long_mode(ui->counter_label_tf_state, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_tf_state, 0, 92);
	lv_obj_set_size(ui->counter_label_tf_state, 128, 16);

	//Write style for counter_label_tf_state, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_tf_state, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_tf_state, lv_color_hex(0x024f5a), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_tf_state, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_tf_state, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_tf_state, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_tf_state, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(ui->counter_label_tf_state, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_tf_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_eth_ip
	ui->counter_label_eth_ip = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_eth_ip, "broker.emqx.io");
	lv_label_set_long_mode(ui->counter_label_eth_ip, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_eth_ip, 0, 66);
	lv_obj_set_size(ui->counter_label_eth_ip, 128, 16);

	//Write style for counter_label_eth_ip, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_eth_ip, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_eth_ip, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_eth_ip, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_eth_ip, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_eth_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_eth_state
	ui->counter_label_eth_state = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_eth_state, "MQTT状态:连接中");
	lv_label_set_long_mode(ui->counter_label_eth_state, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_eth_state, 0, 50);
	lv_obj_set_size(ui->counter_label_eth_state, 128, 16);

	//Write style for counter_label_eth_state, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_eth_state, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_eth_state, lv_color_hex(0x024f5a), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_eth_state, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_eth_state, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_eth_state, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_eth_state, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(ui->counter_label_eth_state, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_eth_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_wifi_ip
	ui->counter_label_wifi_ip = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_wifi_ip, "ip地址未获取");
	lv_label_set_long_mode(ui->counter_label_wifi_ip, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_wifi_ip, 0, 24);
	lv_obj_set_size(ui->counter_label_wifi_ip, 128, 16);

	//Write style for counter_label_wifi_ip, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_wifi_ip, lv_color_hex(0x000000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_wifi_ip, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_wifi_ip, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_wifi_ip, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_wifi_ip, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Write codes counter_label_wifi_state
	ui->counter_label_wifi_state = lv_label_create(ui->counter);
	lv_label_set_text(ui->counter_label_wifi_state, "WIFI状态:FAIL");
	lv_label_set_long_mode(ui->counter_label_wifi_state, LV_LABEL_LONG_WRAP);
	lv_obj_set_pos(ui->counter_label_wifi_state, 0, 8);
	lv_obj_set_size(ui->counter_label_wifi_state, 128, 16);

	//Write style for counter_label_wifi_state, Part: LV_PART_MAIN, State: LV_STATE_DEFAULT.
	lv_obj_set_style_border_width(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_radius(ui->counter_label_wifi_state, 4, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(ui->counter_label_wifi_state, lv_color_hex(0x024f5a), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui->counter_label_wifi_state, &lv_font_simsun_16, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_letter_space(ui->counter_label_wifi_state, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_line_space(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_text_align(ui->counter_label_wifi_state, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui->counter_label_wifi_state, 255, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(ui->counter_label_wifi_state, lv_color_hex(0xff0000), LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_top(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_right(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_bottom(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_pad_left(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);
	lv_obj_set_style_shadow_width(ui->counter_label_wifi_state, 0, LV_PART_MAIN|LV_STATE_DEFAULT);

	//Update current screen layout.
	lv_obj_update_layout(ui->counter);

	
}
