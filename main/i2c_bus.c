#include "i2c_bus.h"

#include "esp_log.h"

#define I2C_BUS_PORT    I2C_NUM_0
#define I2C_BUS_SDA     GPIO_NUM_41
#define I2C_BUS_SCL     GPIO_NUM_42
#define I2C_BUS_HZ      100000

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t s_bus = NULL;

esp_err_t i2c_bus_init(void)
{
    if (s_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = I2C_BUS_SDA,
        .scl_io_num = I2C_BUS_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化失败：%s", esp_err_to_name(err));
        s_bus = NULL;
        return err;
    }

    ESP_LOGI(TAG, "初始化完成，SDA=GPIO%d SCL=GPIO%d", I2C_BUS_SDA, I2C_BUS_SCL);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_get(void)
{
    return s_bus;
}
