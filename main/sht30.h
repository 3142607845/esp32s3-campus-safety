/*
 * SHT30 / SHT31 温湿度传感器驱动（I2C）
 *
 * 接线：VIN -> 3V3，GND -> GND，SDA -> GPIO41，SCL -> GPIO42
 */
#ifndef SHT30_H
#define SHT30_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 I2C 总线并探测 0x44 / 0x45，返回 ESP_OK 表示找到传感器 */
esp_err_t sht30_init(void);

/* 读取一次温湿度：temp_c 单位 ℃，hum_rh 单位 %RH */
esp_err_t sht30_read(float *temp_c, float *hum_rh);

#ifdef __cplusplus
}
#endif

#endif /* SHT30_H */
