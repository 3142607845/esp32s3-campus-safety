#ifndef __APP_RC522_H__
#define __APP_RC522_H__

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* MFRC-522 (RC522) 13.56MHz 射频读卡模块，SPI 接口
 * 注意：模块必须用 3.3V 供电，接 5V 会烧毁 */

esp_err_t app_rc522_init(void);

/* 轮询是否有卡。读到一张新卡时返回 true，并把 UID 写成
 * 形如 "A1B2C3D4" 的字符串（每个字节两位十六进制大写）。
 * 同一张卡持续放在感应区时只返回一次，拿开后再放才会再次返回。 */
bool app_rc522_poll(char *uid_str, size_t len);

#endif
