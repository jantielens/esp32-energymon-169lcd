#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <Arduino.h>
#include <SPI.h>
#include "board_config.h"

// ST7789V2 commands
#define ST7789_CASET   0x2A
#define ST7789_RASET   0x2B
#define ST7789_RAMWR   0x2C

void lcd_init();
void lcd_set_backlight(uint8_t brightness);  // 0-100%
void lcd_write_command(uint8_t cmd);
void lcd_write_data(uint8_t data);
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_fill_screen(uint16_t color);
void lcd_push_colors(uint16_t *data, uint32_t len);

#endif // LCD_DRIVER_H
