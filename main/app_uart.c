#include "app_uart.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "app_uart";

// ESP32-S3 UART1 引脚：GPIO38=RX(接 K230 的 IO40/TXD)，GPIO39=TX(接 K230 的 IO41/RXD)
// 注意：原来用的 GPIO1/GPIO2 已被火焰传感器 AO/DO 占用，必须避开
#define UART_NUM        UART_NUM_1
#define UART_RX_PIN     38
#define UART_TX_PIN     39
#define BUF_SIZE        256
#define LINK_TIMEOUT_MS 2000    // 超过 2 秒没收到数据就认为链路断开

// 火焰状态：-1=还没收到数据或链路超时，0=无火焰，1=有火焰
static int s_fire_state = -1;
// 最近一次收到有效数据的时刻（微秒）
static int64_t s_last_rx_us = 0;
// 行缓冲：串口数据可能被拆成多段，必须攒到换行符再解析
static char s_line[32];
static int s_line_len = 0;

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

// 逐字节拼行，收到 FIRE:x 后更新状态
static void feed_byte(char c)
{
    if (c == '\n' || c == '\r') {
        if (s_line_len >= 5 && strncmp(s_line, "FIRE:", 5) == 0) {
            int v = (s_line[5] == '1') ? 1 : 0;
            if (v != s_fire_state) {
                ESP_LOGW(TAG, "K230 火焰状态 -> %s", v ? "有火焰" : "无火焰");
            }
            s_fire_state = v;
            s_last_rx_us = esp_timer_get_time();
        }
        s_line_len = 0;
        return;
    }
    if (s_line_len < (int)sizeof(s_line) - 1) {
        s_line[s_line_len++] = c;
    } else {
        s_line_len = 0;     // 行太长，丢弃重新同步
    }
}

int app_uart_get_fire(void)
{
    uint8_t data[BUF_SIZE];
    // 非阻塞读取（timeout=0），有数据就处理
    int len = uart_read_bytes(UART_NUM, data, sizeof(data), 0);
    for (int i = 0; i < len; i++) {
        feed_byte((char)data[i]);
    }

    if (s_last_rx_us == 0) {
        return -1;          // 上电后还没收到过数据
    }
    if ((esp_timer_get_time() - s_last_rx_us) / 1000 > LINK_TIMEOUT_MS) {
        if (s_fire_state != -1) {
            ESP_LOGW(TAG, "K230 数据超时，火焰状态按未知处理");
            s_fire_state = -1;
        }
    }
    return s_fire_state;
}
