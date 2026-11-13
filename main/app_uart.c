#include "app_uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_uart";

// ESP32-S3 UART1 引脚：GPIO1=RX(接收K230), GPIO2=TX(发给K230, 可选)
#define UART_NUM      UART_NUM_1
#define UART_RX_PIN   1
#define UART_TX_PIN   2
#define BUF_SIZE      1024

// 全局安全帽状态：-1=无数据, 0=未戴/无人, 1=已戴
static int s_hat_state = -1;

void app_uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM, &cfg);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, BUF_SIZE, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "UART1 init OK: RX=GPIO%d TX=GPIO%d @115200",
             UART_RX_PIN, UART_TX_PIN);
}

int app_uart_get_hat(void)
{
    uint8_t data[BUF_SIZE];
    // 非阻塞读取（timeout=0），有数据就处理
    int len = uart_read_bytes(UART_NUM, data, BUF_SIZE - 1, 0);
    if (len > 0) {
        data[len] = '\0';
        // 查找 "HAT:" 前缀
        char *p = strstr((char *)data, "HAT:");
        if (p != NULL) {
            char c = p[4];  // "HAT:" 后面第1个字符就是 0 或 1
            if (c == '1') {
                s_hat_state = 1;
            } else if (c == '0') {
                s_hat_state = 0;
            }
            ESP_LOGI(TAG, "recv hat state: %d", s_hat_state);
        }
    }
    return s_hat_state;
}
