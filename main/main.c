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
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

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
#include "sht30.h"
#include "sgp30.h"

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

// LD2410C-P 24GHz 人体存在感应雷达：OUT 接 GPIO5（高电平=有人）
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

// 4 线火焰传感器：DO 数字量接 GPIO2（低电平=检测到火焰），AO 模拟量接 GPIO1
#define FLAME_DO_PIN GPIO_NUM_2
#define FLAME_AO_CH  ADC_CHANNEL_0     /* ESP32-S3 的 ADC1_CH0 对应 GPIO1 */

// SG90 舵机 x2（报警时开窗）：窗A 信号接 GPIO8，窗B 信号接 GPIO9
#define SERVO_A_PIN GPIO_NUM_8
#define SERVO_B_PIN GPIO_NUM_9
#define SERVO_A_CH  LEDC_CHANNEL_0
#define SERVO_B_CH  LEDC_CHANNEL_1
#define SERVO_TM    LEDC_TIMER_0
#define SERVO_MODE  LEDC_LOW_SPEED_MODE
#define SERVO_RES   LEDC_TIMER_13_BIT      /* 2^13 = 8192 级 */
#define SERVO_HZ    50                     /* 舵机标准 50Hz，周期 20ms */
#define SERVO_DUTY_MAX 8192
#define WIN_CLOSE_DEG  0                   /* 关窗角度 */
#define WIN_OPEN_DEG   90                  /* 开窗角度 */

TaskHandle_t  task_handle_gui;
esp_timer_create_args_t timer_args_gui;
esp_timer_handle_t timer_handle_gui;
lv_ui guider_ui;

extern MY_WIFI_INFO my_wifi_info;
extern MY_ETH_INFO my_eth_info;
extern MY_SDCARD_INFO my_SDcard_info;
extern MY_MQTT_INFO my_mqtt_info;

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_adc_cali = NULL;

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

/* 设置单路舵机角度：0~180 度 */
static void servo_set_angle(ledc_channel_t ch, int deg)
{
    if (deg < 0)   deg = 0;
    if (deg > 180) deg = 180;

    /* 500us -> 0°，2500us -> 180° */
    uint32_t pulse_us = 500 + (uint32_t)deg * 2000 / 180;
    uint32_t duty = (uint32_t)((uint64_t)pulse_us * SERVO_DUTY_MAX / 20000);

    ledc_set_duty(SERVO_MODE, ch, duty);
    ledc_update_duty(SERVO_MODE, ch);
}

/* 两扇窗统一动作 */
static void window_set(int deg)
{
    servo_set_angle(SERVO_A_CH, deg);
    servo_set_angle(SERVO_B_CH, deg);
    ESP_LOGI(TAG, "窗户 -> %s（%d 度）", deg > 0 ? "开窗" : "关窗", deg);
}

/* 双舵机初始化：两路共用 1 个 50Hz 定时器 */
static void servo_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode      = SERVO_MODE,
        .timer_num       = SERVO_TM,
        .duty_resolution = SERVO_RES,
        .freq_hz         = SERVO_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch_conf = {
        .gpio_num   = SERVO_A_PIN,
        .speed_mode = SERVO_MODE,
        .channel    = SERVO_A_CH,
        .timer_sel  = SERVO_TM,
        .duty       = 0,
        .hpoint     = 0,
        .intr_type  = LEDC_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    ch_conf.gpio_num = SERVO_B_PIN;
    ch_conf.channel  = SERVO_B_CH;
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    window_set(WIN_CLOSE_DEG);   // 上电先关窗
    ESP_LOGI(TAG, "双舵机初始化完成 A=GPIO%d B=GPIO%d（初始关窗）", SERVO_A_PIN, SERVO_B_PIN);
}

/* 火焰传感器初始化：DO 数字输入 + AO 模拟输入 */
static void flame_init(void)
{
    gpio_config_t flame_conf = {
        .pin_bit_mask = (1ULL << FLAME_DO_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&flame_conf);

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, FLAME_AO_CH, &chan_cfg));

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = ADC_UNIT_1,
        .atten    = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_adc_cali) != ESP_OK) {
        s_adc_cali = NULL;
    }
#endif

    ESP_LOGI(TAG, "火焰传感器初始化完成 DO=GPIO%d AO=GPIO1", FLAME_DO_PIN);
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

    // LD2410C-P 人体存在感应雷达初始化（只用 OUT 电平，TX/RX 悬空）
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << PIR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // 下拉使能：浮空时读 0
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);
    ESP_LOGI(TAG, "人体雷达(LD2410C-P) 初始化完成，OUT 引脚 GPIO%d", PIR_PIN);

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

    // 火焰传感器初始化（DO=GPIO2 / AO=GPIO1）
    flame_init();

    // 双舵机初始化（报警时开窗）
    servo_init();

    // SHT30 温湿度传感器初始化（I2C：SDA=GPIO41，SCL=GPIO42）
    if (sht30_init() != ESP_OK) {
        ESP_LOGE(TAG, "SHT30 初始化失败，温湿度将显示为 0");
    }

    // SGP30 气体传感器初始化（与 SHT30 共用 I2C 总线，地址 0x58）
    if (sgp30_init() != ESP_OK) {
        ESP_LOGE(TAG, "SGP30 初始化失败，eCO2/TVOC 将显示为 0");
    }

    // 以下官方工厂测试函数已注释：无 SD 卡时 app_sd_card 会阻塞导致看门狗复位
    // app_eth();
    // app_sd_card();
    // app_gpio();

    int sensor_tick = 0;
    int alarm_state = -1;   // 上一次的报警状态：-1=未初始化，0=正常，1=报警
    int beep_tick = 0;      // 蜂鸣器间歇鸣叫计时
    int beep_on = 0;        // 蜂鸣器当前是否在响
    int ao_tick = 0;        // 火焰 AO 采样分频
    int ao_mv = 0;          // 火焰 AO 电压（mV）
    int pir_last = -1;      // 上一次的人体检测状态：-1=未初始化，0=无人，1=有人
    float last_temp = 0.0f; // 最近一次有效的 SHT30 温度
    float last_hum = 0.0f;  // 最近一次有效的 SHT30 湿度
    int sgp_tick = 0;       // SGP30 每秒测量一次的计时
    uint16_t eco2 = 0;      // 最近一次 eCO2（ppm）
    uint16_t tvoc = 0;      // 最近一次 TVOC（ppb）
    bool sgp_valid = false; // SGP30 是否成功读到过数据

    while (true) {
        // 读取 K230 发来的安全帽检测结果
        int hat = app_uart_get_hat();
        // 读取 LD2410C-P 人体雷达 OUT：1=有人，0=无人
        int pir = gpio_get_level(PIR_PIN);
        // 人体状态一变化立刻打印（主循环 10ms 一拍）
        if (pir != pir_last) {
            pir_last = pir;
            ESP_LOGI(TAG, "【人体检测】%s（电平 %d）", pir ? "有人" : "无人", pir);
        }
        // 读取 MQ-2 烟雾传感器：0=检测到烟雾，1=正常
        int mq2_raw = gpio_get_level(MQ2_PIN);
        // 1=烟雾报警（真实传感器，低电平有效）
        int smoke = (mq2_raw == 0) ? 1 : 0;
        // 读取火焰传感器 DO：0=检测到火焰，1=正常
        int flame_do = gpio_get_level(FLAME_DO_PIN);
        int flame = (flame_do == 0) ? 1 : 0;

        // 火焰 AO 每 100ms 采一次（只用于日志观察强弱，报警判断仍用 DO）
        ao_tick++;
        if (ao_tick >= 10) {
            ao_tick = 0;
            int ao_raw = 0;
            adc_oneshot_read(s_adc, FLAME_AO_CH, &ao_raw);
            if (s_adc_cali) {
                adc_cali_raw_to_voltage(s_adc_cali, ao_raw, &ao_mv);
            }
        }

        // ===== 报警联动：火焰优先 =====
        //   2 = 检测到火焰（红灯 + 蜂鸣器）
        //   1 = 只检测到烟雾（绿灯，蜂鸣器不响）
        //   0 = 正常（灯不亮，蜂鸣器不响）
        int alarm = flame ? 2 : (smoke ? 1 : 0);

        if (alarm != alarm_state) {
            alarm_state = alarm;
            if (alarm == 2) {
                // 火焰：红灯亮 + 绿色灭 + 蜂鸣器响 + 风扇开 + 开窗
                gpio_set_level(LED_R_PIN, 1);
                gpio_set_level(LED_G_PIN, 0);
                gpio_set_level(FAN_PIN, 1);
                gpio_set_level(BUZZER_PIN, 1);
                window_set(WIN_OPEN_DEG);
                beep_tick = 0;
                beep_on = 1;
                ESP_LOGW(TAG, "【火焰报警】烟雾=%d 火焰=%d -> 红灯亮 蜂鸣器响 风扇开 开窗",
                         smoke, flame);
            } else if (alarm == 1) {
                // 只有烟雾：绿灯亮 + 红色灭 + 蜂鸣器静音 + 风扇开 + 开窗
                gpio_set_level(LED_R_PIN, 0);
                gpio_set_level(LED_G_PIN, 1);
                gpio_set_level(FAN_PIN, 1);
                gpio_set_level(BUZZER_PIN, 0);
                window_set(WIN_OPEN_DEG);
                beep_on = 0;
                ESP_LOGI(TAG, "【烟雾报警】-> 绿灯亮 蜂鸣器静音 风扇开 开窗");
            } else {
                // 正常：灯灭 + 风扇关 + 蜂鸣器静音 + 关窗
                gpio_set_level(LED_R_PIN, 0);
                gpio_set_level(LED_G_PIN, 0);
                gpio_set_level(FAN_PIN, 0);
                gpio_set_level(BUZZER_PIN, 0);
                window_set(WIN_CLOSE_DEG);
                beep_on = 0;
                ESP_LOGI(TAG, "【正常】烟雾和火焰都消失 -> 灯灭 风扇关 蜂鸣器静音 关窗");
            }
        }

        // 只有火焰报警期间蜂鸣器间歇鸣叫：响 500ms / 停 500ms（主循环 10ms 一拍）
        if (alarm_state == 2) {
            beep_tick++;
            if (beep_tick >= 50) {
                beep_tick = 0;
                beep_on = !beep_on;
                gpio_set_level(BUZZER_PIN, beep_on);
            }
        }

        // 调试：每秒打印一次各传感器原始电平
        static int pir_dbg_tick = 0;
        pir_dbg_tick++;
        if (pir_dbg_tick >= 100) {
            pir_dbg_tick = 0;
            ESP_LOGI(TAG, "PIR 原始电平 = %d -> %s", pir, pir ? "有人" : "无人");
            ESP_LOGI(TAG, "MQ-2 原始电平 = %d -> %s", mq2_raw,
                     smoke ? "报警" : "正常");
            ESP_LOGI(TAG, "火焰 DO = %d -> %s，AO = %d mV", flame_do,
                     flame ? "有火焰" : "无火焰", ao_mv);
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

        // SGP30 要求约 1 秒测量一次（主循环 10ms 一拍，100 拍 = 1 秒）
        sgp_tick++;
        if (sgp_tick >= 100) {
            sgp_tick = 0;
            uint16_t e = 0, v = 0;
            if (sgp30_read(&e, &v) == ESP_OK) {
                eco2 = e;
                tvoc = v;
                sgp_valid = true;
            }
        }

        // 每 5 秒上报一次（温湿度来自 SHT30 实测，eCO2/TVOC 来自 SGP30）
        sensor_tick++;
        if (sensor_tick >= 500) {
            sensor_tick = 0;

            // 读 SHT30；读失败就沿用上一次的有效值，避免数值乱跳
            float tc = 0.0f, rh = 0.0f;
            if (sht30_read(&tc, &rh) == ESP_OK) {
                last_temp = tc;
                last_hum = rh;
            } else {
                ESP_LOGW(TAG, "SHT30 读取失败，沿用上一次数值 %.1fC / %.1f%%",
                         last_temp, last_hum);
            }
            int t = (int)(last_temp * 10.0f + 0.5f);
            int h = (int)(last_hum * 10.0f + 0.5f);

            char payload[160];
            snprintf(payload, sizeof(payload),
                     "{\"temp\":%d.%d,\"hum\":%d.%d,\"eco2\":%u,\"tvoc\":%u,"
                     "\"smoke\":%d,\"hat\":%d,\"pir\":%d,\"flame\":%d}",
                     t/10, t%10, h/10, h%10, eco2, tvoc, smoke, hat, pir, flame);
            app_mqtt_publish(MQTT_PUB_TOPIC, payload, strlen(payload));
            ESP_LOGI(TAG, "上报 %s", payload);
            if (sgp_valid) {
                ESP_LOGI(TAG, "SGP30 eCO2=%u ppm  TVOC=%u ppb", eco2, tvoc);
            } else {
                ESP_LOGW(TAG, "SGP30 无数据（0x58 未识别或读取失败），eco2/tvoc 保持 0");
            }

            char buf[48];
            snprintf(buf, sizeof(buf), "Temp:%d.%dC", t/10, t%10);
            lv_label_set_text(guider_ui.counter_label_tf_state, buf);
            snprintf(buf, sizeof(buf), "Humi:%d.%d%%", h/10, h%10);
            lv_label_set_text(guider_ui.counter_label_tf_rw, buf);
            // 最后一行显示 烟雾 / 火焰 / 人体 状态（缩写避免超出屏幕宽度）
            snprintf(buf, sizeof(buf), "S:%s F:%s P:%s",
                     smoke ? "ALM" : "OK", flame ? "ALM" : "OK", pir ? "Y" : "N");
            lv_label_set_text(guider_ui.counter_label_gpio, buf);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
