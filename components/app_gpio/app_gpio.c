/*
 * app_gpio.c
 *
 *  Created on: 2024年1月15日
 *      Author: love
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define TEST_PIN_SEL (1ULL << GPIO_NUM_1)  \
                    |(1ULL << GPIO_NUM_2)  \
                    |(1ULL << GPIO_NUM_3)  \
                    |(1ULL << GPIO_NUM_4)  \
                    |(1ULL << GPIO_NUM_5)  \
                    |(1ULL << GPIO_NUM_6)  \
                    |(1ULL << GPIO_NUM_7)  \
                    |(1ULL << GPIO_NUM_8)  \
                    |(1ULL << GPIO_NUM_9)  \
                    |(1ULL << GPIO_NUM_19) \
                    |(1ULL << GPIO_NUM_20) \
                    |(1ULL << GPIO_NUM_39) \
                    |(1ULL << GPIO_NUM_40) \
                    |(1ULL << GPIO_NUM_41) \
                    |(1ULL << GPIO_NUM_42) \
                    |(1ULL << GPIO_NUM_47) \
                    |(1ULL << GPIO_NUM_48)

int gpio_num[] = {GPIO_NUM_1,  GPIO_NUM_2,  GPIO_NUM_3,  GPIO_NUM_4,  GPIO_NUM_5,  GPIO_NUM_6,  \
                  GPIO_NUM_7,  GPIO_NUM_8,  GPIO_NUM_9,  GPIO_NUM_39, GPIO_NUM_40, GPIO_NUM_41, \
                  GPIO_NUM_42, GPIO_NUM_47, GPIO_NUM_48, GPIO_NUM_19, GPIO_NUM_20};
int num_i = 0,num_size;

void app_gpio_test(void *arg)
{
    gpio_config_t IO_Config;

    IO_Config.pin_bit_mask = TEST_PIN_SEL;
    IO_Config.mode = GPIO_MODE_DEF_OUTPUT;       /*输出 */ // @suppress("Symbol is not resolved")
    IO_Config.pull_up_en = 0;                    /*上拉*/
    IO_Config.pull_down_en = 0;
    IO_Config.intr_type = GPIO_INTR_DISABLE;     /*不使能中断*/
    gpio_config(&IO_Config);

    num_size = sizeof(gpio_num)/4;

    printf("GPIO size is: %d.\r\n",num_size);

    for(num_i = 0; num_i < num_size; num_i++){
        gpio_set_level(gpio_num[num_i],1);
    }

    while(true){
        for(num_i = 0; num_i < num_size; num_i++){
            gpio_set_level(gpio_num[num_i],0);
            vTaskDelay(pdMS_TO_TICKS(200));
            gpio_set_level(gpio_num[num_i],1);
        }
    }
}
TaskHandle_t  task_handle_gpio;
void app_gpio(void)
{
    BaseType_t status = xTaskCreate(app_gpio_test, "gpio_run_task", 4096, NULL, 5, &task_handle_gpio);
    if (status == pdPASS) {
        ESP_LOGI("APP_GPIO","Display xTaskCreate OK!");
    }
}

