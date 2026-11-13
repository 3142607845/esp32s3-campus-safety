/*
 * app_sd_card.h
 *
 *  Created on: 2024年1月15日
 *      Author: love
 */

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

typedef struct {
    char flag;
    char first;
    char string[128];
} MY_SDCARD_INFO;

void app_sd_card(void);

#endif /* COMPONENTS_APP_SD_CARD_APP_SD_CARD_H_ */
