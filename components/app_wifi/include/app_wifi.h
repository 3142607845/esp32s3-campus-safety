/*
 * wifi_app.h
 *
 *  Created on: 2024年1月12日
 *      Author: love
 */

#ifndef _APP_WIFI_H_
#define _APP_WIFI_H_

#include "string.h"

typedef struct {
    char flag;
    char first;
    char ip_string[64];
} MY_WIFI_INFO;

void app_wifi(uint8_t *name, uint8_t *password);

#endif /* COMPONENTS_WIFI_APP_INCLUDE_WIFI_APP_H_ */
