/*
 * app_eth.h
 *
 *  Created on: 2024年1月14日
 *      Author: love
 */

#ifndef COMPONENTS_APP_ETHERNET_APP_ETH_H_
#define COMPONENTS_APP_ETHERNET_APP_ETH_H_

typedef struct {
    char flag;
    char first;
    char ip_string[64];
} MY_ETH_INFO;

void app_eth(void);


#endif /* COMPONENTS_APP_ETHERNET_APP_ETH_H_ */
