/*
 * main_mqtt.c
 * 连接 EMQX 公共 MQTT 服务器（broker.emqx.io:1883），上报JSON数据
 */

#include "main_mqtt.h"

#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "main_mqtt";

#define MQTT_BROKER_URI "mqtt://broker.emqx.io:1883"

static esp_mqtt_client_handle_t s_client = NULL;
MY_MQTT_INFO my_mqtt_info;

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT 已连接");
            my_mqtt_info.connected = 1;
            my_mqtt_info.flag = 1;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT 断开连接");
            my_mqtt_info.connected = 0;
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT 连接错误");
            my_mqtt_info.connected = 0;
            break;
        default:
            break;
    }
}

void app_mqtt_start(void)
{
    my_mqtt_info.flag = 0;
    my_mqtt_info.connected = 0;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "MQTT 客户端已启动");
}

int app_mqtt_publish(const char *topic, const char *data, int len)
{
    if (s_client == NULL || my_mqtt_info.connected == 0) {
        return -1;
    }
    return esp_mqtt_client_publish(s_client, topic, data, len, 0, 0);
}