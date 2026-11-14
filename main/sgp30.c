#include "sgp30.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#include "i2c_bus.h"

#define SGP30_ADDR        0x58
#define SGP30_I2C_HZ      100000

#define CMD_IAQ_INIT      0x2003
#define CMD_MEASURE_IAQ   0x2008

static const char *TAG = "sgp30";

static i2c_master_dev_handle_t s_dev = NULL;
static int s_fail_cnt = 0;   /* 连续失败次数，用于自动恢复 */

/* SGP30 数据 CRC8：多项式 0x31，初值 0xFF */
static uint8_t sgp_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t sgp_write_cmd(uint16_t cmd)
{
    uint8_t buf[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

esp_err_t sgp30_init(void)
{
    esp_err_t err = i2c_bus_init();
    if (err != ESP_OK) {
        return err;
    }

    i2c_master_bus_handle_t bus = i2c_bus_get();
    if (bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (i2c_master_probe(bus, SGP30_ADDR, 100) != ESP_OK) {
        ESP_LOGE(TAG, "地址 0x58 无应答，检查 VCC(3V3)/GND/SDA/SCL");
        /* 扫一遍总线，帮助区分"总线整体不通"和"只有 SGP30 不应答" */
        int found = 0;
        for (uint8_t a = 0x08; a < 0x78; a++) {
            if (i2c_master_probe(bus, a, 20) == ESP_OK) {
                ESP_LOGW(TAG, "总线扫描：0x%02X 有应答", a);
                found++;
            }
        }
        if (found == 0) {
            ESP_LOGE(TAG, "总线扫描：一个设备都没有，检查 3V3 供电和共地");
        }
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SGP30_ADDR,
        .scl_speed_hz = SGP30_I2C_HZ,
    };
    err = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "添加设备失败：%s", esp_err_to_name(err));
        return err;
    }

    err = sgp_write_cmd(CMD_IAQ_INIT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "iaq_init 失败：%s", esp_err_to_name(err));
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    s_fail_cnt = 0;
    ESP_LOGI(TAG, "初始化完成，地址 0x58（前 15 秒为预热值）");
    return ESP_OK;
}

esp_err_t sgp30_read(uint16_t *eco2, uint16_t *tvoc)
{
    if (s_dev == NULL) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            ESP_LOGW(TAG, "传感器未初始化（0x58 未识别），eCO2/TVOC 不会更新");
        }
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t buf[6] = {0};

    esp_err_t err = sgp_write_cmd(CMD_MEASURE_IAQ);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "发送测量命令失败：%s", esp_err_to_name(err));
        goto fail;
    }

    vTaskDelay(pdMS_TO_TICKS(20));   /* 测量最长 12ms，留余量 */

    err = i2c_master_receive(s_dev, buf, sizeof(buf), 100);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "读取数据失败：%s", esp_err_to_name(err));
        goto fail;
    }

    if (sgp_crc8(buf, 2) != buf[2] || sgp_crc8(buf + 3, 2) != buf[5]) {
        ESP_LOGW(TAG, "CRC 校验失败，原始数据 %02X %02X %02X %02X %02X %02X",
                 buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
        err = ESP_ERR_INVALID_CRC;
        goto fail;
    }

    s_fail_cnt = 0;
    *eco2 = ((uint16_t)buf[0] << 8) | buf[1];
    *tvoc = ((uint16_t)buf[3] << 8) | buf[4];
    return ESP_OK;

fail:
    /* 只统计失败次数，不在读失败时动作，避免再给总线增加负担 */
    if (++s_fail_cnt == 5) {
        ESP_LOGW(TAG, "已连续失败 5 次，先检查供电与接线");
    }
    return err;
}
