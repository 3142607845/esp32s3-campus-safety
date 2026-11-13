#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "driver/gpio.h"

#include "lcd.h"
#include "lv_port_disp_template.h"
#include "gui_guider.h"
#include "custom.h"

#include "app_wifi.h"
#include "app_eth.h"
#include "app_sd_card.h"
#include "app_gpio.h"
#include "main_mqtt.h"
#include "app_uart.h"

/* WiFi 账号密码放在 main/wifi_config.h 里，该文件已被 .gitignore 忽略
   克隆本仓库后请把 wifi_config.h.example 复制成 wifi_config.h 并填写自己的账号 */
#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"
#endif

static const char *TAG = "main output";

#define MQTT_PUB_TOPIC "aibot/esp32s3/status"

// HC-SR501 人体红外传感器接 GPIO5
#define PIR_PIN GPIO_NUM_5
// 继电器（控制风扇）接 GPIO6
#define FAN_PIN GPIO_NUM_6
// MQ-2 烟雾传感器 DO 接 GPIO4（低电平=检测到烟雾）
#define MQ2_PIN GPIO_NUM_4
// 红绿双色 LED（共阴）：红灯阳极接 GPIO10，绿灯阳极接 GPIO47
#define LED_R_PIN GPIO_NUM_10
#define LED_G_PIN GPIO_NUM_47
// 有源蜂鸣器模块（S8050 驱动，高电平触发）I/O 接 GPIO7
#define BUZZER_PIN GPIO_NUM_7

TaskHandle_t  task_handle_gui;
esp_timer_create_args_t timer_args_gui;
esp_timer_handle_t timer_handle_gui;
lv_ui guider_ui;

extern MY_WIFI_INFO my_wifi_info;
extern MY_ETH_INFO my_eth_info;
extern MY_SDCARD_INFO my_SDcard_info;
extern MY_MQTT_INFO my_mqtt_info;

static void lv_tick_task(void *arg)
{
    lv_tick_inc(1);
}
void lv_run_task(void *arg)
{
    while (true) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
	LCD_Init();
    lv_init();
    lv_port_disp_init();
    setup_ui(&guider_ui);
    timer_args_gui.callback = &lv_tick_task;
    timer_args_gui.name = "periodic_gui";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args_gui, &timer_handle_gui));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle_gui, 1 * 1000));

    BaseType_t status = xTaskCreate(lv_run_task, "lv_run_task", 4096, NULL, 2, &task_handle_gui);
    if (status == pdPASS) {
        ESP_LOGI(TAG,"Display xTaskCreate OK!");
    }
    //WIFI初始化
    app_wifi((uint8_t*)WIFI_SSID, (uint8_t*)WIFI_PASS);
    //MQTT上云（WiFi连上之后）
    app_mqtt_start();
    //UART1 初始化（接收 K230 视觉模块的安全帽检测结果）
    app_uart_init();

    // HC-SR501 人体红外传感器初始化
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // 下拉使能：浮空时读 0
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);
    ESP_LOGI(TAG, "PIR(HC-SR501) 初始化完成，引脚 GPIO%d", PIR_PIN);

    // 继电器（控制风扇）初始化
    gpio_config_t fan_conf = {
        .pin_bit_mask = (1ULL << FAN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&fan_conf);
    gpio_set_level(FAN_PIN, 0);   // 初始关闭风扇
    ESP_LOGI(TAG, "继电器(风扇) 初始化完成，引脚 GPIO%d", FAN_PIN);

    // MQ-2 烟雾传感器初始化（DO 数字输出）
    gpio_config_t mq2_conf = {
        .pin_bit_mask = (1ULL << MQ2_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,      // 上拉：DO 开漏时默认高
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&mq2_conf);
    ESP_LOGI(TAG, "MQ-2 初始化完成，引脚 GPIO%d", MQ2_PIN);

    // 红绿双色 LED（共阴）初始化，红/绿阳极各自独立控制
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_R_PIN) | (1ULL << LED_G_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED_R_PIN, 0);
    gpio_set_level(LED_G_PIN, 0);   // 上电默认灯灭（无烟雾）
    ESP_LOGI(TAG, "双色LED初始化完成 R=GPIO%d G=GPIO%d", LED_R_PIN, LED_G_PIN);

    // 有源蜂鸣器初始化（高电平触发，S8050 三极管驱动）
    gpio_config_t buzzer_conf = {
        .pin_bit_mask = (1ULL << BUZZER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&buzzer_conf);
    gpio_set_level(BUZZER_PIN, 0);   // 初始静音
    ESP_LOGI(TAG, "蜂鸣器初始化完成，信号脚 GPIO%d", BUZZER_PIN);
    // 以下官方工厂测试函数已注释：无 SD 卡时 app_sd_card 会阻塞导致看门狗复位
    // app_eth();
    // app_sd_card();
    // app_gpio();

    int sensor_tick = 0;
    int alarm_state = -1;   // 上一次的报警状态：-1=未初始化，0=正常，1=报警
    int beep_tick = 0;      // 蜂鸣器间歇鸣叫计时
    int beep_on = 0;        // 蜂鸣器当前是否在响
    while (true) {
        // 读取 K230 发来的安全帽检测结果
        int hat = app_uart_get_hat();
        // 读取 HC-SR501 人体红外：1=有人，0=无人
        int pir = gpio_get_level(PIR_PIN);
        // 读取 MQ-2 烟雾传感器：0=检测到烟雾，1=正常
        int mq2_raw = gpio_get_level(MQ2_PIN);
        // 1=烟雾报警（真实传感器，低电平有效）
        int smoke = (mq2_raw == 0) ? 1 : 0;

        // ===== 报警联动：MQ-2 是唯一触发源 =====
        if (smoke != alarm_state) {
            alarm_state = smoke;
            if (smoke) {
                // 有烟雾：红灯常亮 + 绿灯灭 + 风扇开 + 蜂鸣器响
                gpio_set_level(LED_R_PIN, 1);
                gpio_set_level(LED_G_PIN, 0);
                gpio_set_level(FAN_PIN, 1);
                gpio_set_level(BUZZER_PIN, 1);
                beep_tick = 0;
                beep_on = 1;
                ESP_LOGI(TAG, "【报警】检测到烟雾 -> 红灯亮 风扇开 蜂鸣器响");
            } else {
                // 无烟雾：灯灭 + 风扇关 + 蜂鸣器静音
                gpio_set_level(LED_R_PIN, 0);
                gpio_set_level(LED_G_PIN, 0);
                gpio_set_level(FAN_PIN, 0);
                gpio_set_level(BUZZER_PIN, 0);
                beep_on = 0;
                ESP_LOGI(TAG, "【正常】无烟雾 -> 灯灭 风扇关 蜂鸣器静音");
            }
        }

        // 报警期间蜂鸣器间歇鸣叫：响 500ms / 停 500ms（主循环 10ms 一拍）
        if (alarm_state == 1) {
            beep_tick++;
            if (beep_tick >= 50) {
                beep_tick = 0;
                beep_on = !beep_on;
                gpio_set_level(BUZZER_PIN, beep_on);
            }
        }

        // 调试：每秒打印一次 PIR / MQ-2 原始电平
        static int pir_dbg_tick = 0;
        pir_dbg_tick++;
        if (pir_dbg_tick >= 100) {
            pir_dbg_tick = 0;
            ESP_LOGI(TAG, "PIR 原始电平 = %d", pir);
            ESP_LOGI(TAG, "MQ-2 原始电平 = %d -> %s", mq2_raw,
                     smoke ? "报警" : "正常");
        }

        if(my_wifi_info.flag == 1){
            my_wifi_info.flag = 0;
            lv_obj_set_style_bg_color(guider_ui.counter_label_wifi_state,
                    lv_color_hex(0xff00), LV_PART_MAIN|LV_STATE_DEFAULT); // @suppress("Symbol is not resolved")
            lv_label_set_text(guider_ui.counter_label_wifi_state, "WIFI状态:OK");
            lv_label_set_text(guider_ui.counter_label_wifi_ip, my_wifi_info.ip_string);
        }

        // MQTT 状态显示（复用 ETH 的 label 位置）
        if(my_mqtt_info.flag == 1){
            my_mqtt_info.flag = 0;
            lv_obj_set_style_bg_color(guider_ui.counter_label_eth_state,
                    lv_color_hex(0xff00), LV_PART_MAIN|LV_STATE_DEFAULT);
            lv_label_set_text(guider_ui.counter_label_eth_state, "MQTT状态:OK");
            lv_label_set_text(guider_ui.counter_label_eth_ip, "broker.emqx.io");
        }

        // 每 5 秒上报一次（温湿度仍为模拟值，烟雾为真实传感器）
        sensor_tick++;
        if (sensor_tick >= 500) {
            sensor_tick = 0;
            int t = 200 + (esp_random() % 150);   // 20.0 ~ 34.9 ℃
            int h = 400 + (esp_random() % 400);   // 40.0 ~ 79.9 %

            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"temp\":%d.%d,\"hum\":%d.%d,\"smoke\":%d,\"hat\":%d,\"pir\":%d}",
                     t/10, t%10, h/10, h%10, smoke, hat, pir);
            app_mqtt_publish(MQTT_PUB_TOPIC, payload, strlen(payload));
            ESP_LOGI(TAG, "上报 %s", payload);

            char buf[48];
            snprintf(buf, sizeof(buf), "Temp:%d.%dC", t/10, t%10);
            lv_label_set_text(guider_ui.counter_label_tf_state, buf);
            snprintf(buf, sizeof(buf), "Humi:%d.%d%%", h/10, h%10);
            lv_label_set_text(guider_ui.counter_label_tf_rw, buf);
            // 最后一行显示烟雾报警 + 人体红外状态
            snprintf(buf, sizeof(buf), "SMK:%s PIR:%s",
                     smoke ? "ALM" : "OK", pir ? "Y" : "N");
            lv_label_set_text(guider_ui.counter_label_gpio, buf);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
