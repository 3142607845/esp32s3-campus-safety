/*
 * I2C 总线公共模块（SHT30 与 SGP30 共用一条总线）
 * SDA = GPIO41，SCL = GPIO42，100kHz
 */
#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化总线，重复调用只会初始化一次 */
esp_err_t i2c_bus_init(void);

/* 获取总线句柄，未初始化时返回 NULL */
i2c_master_bus_handle_t i2c_bus_get(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_BUS_H */
