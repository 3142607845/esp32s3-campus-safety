/*
 * SGP30 气体传感器驱动（eCO2 / TVOC）
 *
 * 与 SHT30 共用 I2C 总线（SDA=GPIO41，SCL=GPIO42）
 * 注意：SGP30 需要每 1 秒测量一次，前 15 秒输出固定 400ppm / 0ppb
 */
#ifndef SGP30_H
#define SGP30_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化：挂到公共 I2C 总线并执行 iaq_init */
esp_err_t sgp30_init(void);

/* 读一次空气质量：eco2 单位 ppm，tvoc 单位 ppb */
esp_err_t sgp30_read(uint16_t *eco2, uint16_t *tvoc);

#ifdef __cplusplus
}
#endif

#endif /* SGP30_H */
