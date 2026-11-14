#include "sht30.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define SHT_I2C_PORT    I2C_NUM_0
#define SHT_SDA_GPIO    GPIO_NUM_41
#define SHT_SCL_GPIO    GPIO_NUM_42
#define SHT_I2C_HZ      100000

#define SHT_ADDR_LOW    0x44
#define SHT_ADDR_HIGH   0x45

static const char *TAG = "sht30";

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;

/* SHT3x 数据 CRC8：多项式 0x31，初值 0xFF */
static uint8_t sht_crc8(const uint8_t *data, int len)
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

esp_err_t sht30_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = SHT_I2C_PORT,
        .sda_io_num = SHT_SDA_GPIO,
        .scl_io_num = SHT_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C 总线初始化失败：%s", esp_err_to_name(err));
        return err;
    }

    uint8_t addr = 0;
    if (i2c_master_probe(s_bus, SHT_ADDR_LOW, 100) == ESP_OK) {
        addr = SHT_ADDR_LOW;
    } else if (i2c_master_probe(s_bus, SHT_ADDR_HIGH, 100) == ESP_OK) {
        addr = SHT_ADDR_HIGH;
    } else {
        ESP_LOGE(TAG, "未找到传感器（0x44 / 0x45 无应答），检查 SDA/SCL 与供电");
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = SHT_I2C_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "添加 I2C 设备失败：%s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "初始化完成，地址 0x%02X，SDA=GPIO%d SCL=GPIO%d",
             addr, SHT_SDA_GPIO, SHT_SCL_GPIO);
    return ESP_OK;
}

esp_err_t sht30_read(float *temp_c, float *hum_rh)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 0x2C06：高重复性测量，不启用时钟拉伸 */
    uint8_t cmd[2] = {0x2C, 0x06};
    uint8_t buf[6] = {0};

    esp_err_t err = i2c_master_transmit(s_dev, cmd, sizeof(cmd), 100);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(20));   /* 高重复性最长 15ms */

    err = i2c_master_receive(s_dev, buf, sizeof(buf), 100);
    if (err != ESP_OK) {
        return err;
    }

    if (sht_crc8(buf, 2) != buf[2] || sht_crc8(buf + 3, 2) != buf[5]) {
        return ESP_ERR_INVALID_CRC;
    }

    uint16_t raw_t = ((uint16_t)buf[0] << 8) | buf[1];
    uint16_t raw_h = ((uint16_t)buf[3] << 8) | buf[4];

    *temp_c = -45.0f + 175.0f * (float)raw_t / 65535.0f;
    *hum_rh = 100.0f * (float)raw_h / 65535.0f;
    return ESP_OK;
}
