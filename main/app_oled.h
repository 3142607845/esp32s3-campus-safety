#ifndef __APP_OLED_H__
#define __APP_OLED_H__

#include <stdbool.h>
#include "esp_err.h"

/* 第二块显示屏：1.3 寸 128x64 I2C OLED（SSD1306 / SH1106，4 针）
 * 与 SHT30 / SGP30 共用 I2C 总线：SDA=GPIO41，SCL=GPIO42
 * 接线：GND->GND  VCC->3.3V  SCL->GPIO42  SDA->GPIO41
 *
 * 目前显示 4 行内容：
 *      实验室安全监测
 *      风扇: 开 / 关
 *      窗户: 开 / 关
 *      刷卡: N 次
 */

esp_err_t app_oled_init(void);

/* 刷新状态页（风扇、窗户、刷卡次数）。
 * 内容没有变化时不会重刷屏幕，可以放在主循环里每轮调用。 */
void app_oled_show_status(bool fan_on, bool window_open, int card_count);

#endif /* __APP_OLED_H__ */
