/*
 * main_mqtt.h
 * 在官方工厂测试项目基础上新增的 MQTT 上云功能
 */

#ifndef _MAIN_MQTT_H_
#define _MAIN_MQTT_H_

#include <stdint.h>

typedef struct {
    char flag;        // =1 表示刚连上 MQTT（供 main 显示后清零）
    char connected;   // 1 = 已连接
} MY_MQTT_INFO;

// 启动 MQTT 客户端（需在 WiFi 连上之后调用）
void app_mqtt_start(void);

// 发布一条消息
int app_mqtt_publish(const char *topic, const char *data, int len);

#endif