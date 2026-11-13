/*
 * esp_spi_lcd.h
 *
 *  Created on: 2024年1月7日
 *      Author: love
 */

#ifndef _ESP_SPI_LCD_H_
#define _ESP_SPI_LCD_H_ 1

#include "lcd.h"
#include "esp_err.h"
#include "driver/spi_master.h"
#include "string.h"

void lcd_spi_init(void);
esp_err_t spi_write(spi_device_handle_t spi, uint8_t *data, uint16_t len);
esp_err_t spi_read(spi_device_handle_t spi, uint8_t *data);
void* spi_memory_malloc(int size);
void spi_memory_free(void *ptr);

#endif /* COMPONENTS_LCD_INCLUDE_ESP_SPI_LCD_H_ */
