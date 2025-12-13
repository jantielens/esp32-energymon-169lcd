#include "lcd_driver.h"

static SPIClass *spi = &SPI;

void lcd_write_command(uint8_t cmd) {
    digitalWrite(LCD_DC_PIN, LOW);
    digitalWrite(LCD_CS_PIN, LOW);
    spi->transfer(cmd);
    digitalWrite(LCD_CS_PIN, HIGH);
}

void lcd_write_data(uint8_t data) {
    digitalWrite(LCD_DC_PIN, HIGH);
    digitalWrite(LCD_CS_PIN, LOW);
    spi->transfer(data);
    digitalWrite(LCD_CS_PIN, HIGH);
}

void lcd_set_backlight(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    // Simple Arduino-style PWM (matches Waveshare sample behavior)
    analogWrite(LCD_BL_PIN, (brightness * 255) / 100);
}

void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // 1.69" module uses 20px Y offset (panel always in portrait mode)
    const uint16_t y_offset = 20;
    const uint16_t x_offset = 0;

    lcd_write_command(ST7789_CASET);
    lcd_write_data((x0 + x_offset) >> 8);
    lcd_write_data((x0 + x_offset) & 0xFF);
    lcd_write_data((x1 + x_offset) >> 8);
    lcd_write_data((x1 + x_offset) & 0xFF);

    lcd_write_command(ST7789_RASET);
    lcd_write_data((y0 + y_offset) >> 8);
    lcd_write_data((y0 + y_offset) & 0xFF);
    lcd_write_data((y1 + y_offset) >> 8);
    lcd_write_data((y1 + y_offset) & 0xFF);

    lcd_write_command(ST7789_RAMWR);
}

void lcd_fill_screen(uint16_t color) {
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

    digitalWrite(LCD_DC_PIN, HIGH);
    digitalWrite(LCD_CS_PIN, LOW);

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    for (uint32_t i = 0; i < (uint32_t)LCD_WIDTH * LCD_HEIGHT; i++) {
        spi->transfer(hi);
        spi->transfer(lo);
    }

    digitalWrite(LCD_CS_PIN, HIGH);
}

void lcd_push_colors(uint16_t *data, uint32_t len) {
    digitalWrite(LCD_DC_PIN, HIGH);
    digitalWrite(LCD_CS_PIN, LOW);

    for (uint32_t i = 0; i < len; i++) {
        spi->transfer((uint8_t)(data[i] >> 8));
        spi->transfer((uint8_t)(data[i] & 0xFF));
    }

    digitalWrite(LCD_CS_PIN, HIGH);
}

void lcd_init() {
    pinMode(LCD_CS_PIN, OUTPUT);
    pinMode(LCD_DC_PIN, OUTPUT);
    pinMode(LCD_RST_PIN, OUTPUT);
    pinMode(LCD_BL_PIN, OUTPUT);

    digitalWrite(LCD_CS_PIN, HIGH);
    digitalWrite(LCD_DC_PIN, HIGH);

    // Start backlight off until init completes
    analogWrite(LCD_BL_PIN, 0);

    // SPI (Mode 3, MSB first) per Waveshare sample
    spi->begin(LCD_SCK_PIN, -1, LCD_MOSI_PIN, LCD_CS_PIN);
    spi->setDataMode(SPI_MODE3);
    spi->setBitOrder(MSBFIRST);
    spi->setFrequency(60000000); // 60MHz - within ST7789 spec

    // Hardware reset (matches sample timing)
    digitalWrite(LCD_CS_PIN, LOW);
    delay(20);
    digitalWrite(LCD_RST_PIN, LOW);
    delay(20);
    digitalWrite(LCD_RST_PIN, HIGH);
    delay(120);

    // ST7789V2 init sequence (from Waveshare sample)
    // BGR mode (manufacturer default) - display_manager.cpp swaps RGB→BGR in flush callback
    // This allows LVGL to blend colors correctly in RGB space, fixing anti-aliasing artifacts
    lcd_write_command(0x36);
    lcd_write_data(0x00);  // BGR mode (bit 3 = 0), portrait orientation

    lcd_write_command(0x3A);
    lcd_write_data(0x05);

    lcd_write_command(0xB2);
    lcd_write_data(0x0B);
    lcd_write_data(0x0B);
    lcd_write_data(0x00);
    lcd_write_data(0x33);
    lcd_write_data(0x35);

    lcd_write_command(0xB7);
    lcd_write_data(0x11);

    lcd_write_command(0xBB);
    lcd_write_data(0x35);

    lcd_write_command(0xC0);
    lcd_write_data(0x2C);

    lcd_write_command(0xC2);
    lcd_write_data(0x01);

    lcd_write_command(0xC3);
    lcd_write_data(0x0D);

    lcd_write_command(0xC4);
    lcd_write_data(0x20);

    lcd_write_command(0xC6);
    lcd_write_data(0x13);

    lcd_write_command(0xD0);
    lcd_write_data(0xA4);
    lcd_write_data(0xA1);

    lcd_write_command(0xD6);
    lcd_write_data(0xA1);

    lcd_write_command(0xE0);
    lcd_write_data(0xF0);
    lcd_write_data(0x06);
    lcd_write_data(0x0B);
    lcd_write_data(0x0A);
    lcd_write_data(0x09);
    lcd_write_data(0x26);
    lcd_write_data(0x29);
    lcd_write_data(0x33);
    lcd_write_data(0x41);
    lcd_write_data(0x18);
    lcd_write_data(0x16);
    lcd_write_data(0x15);
    lcd_write_data(0x29);
    lcd_write_data(0x2D);

    lcd_write_command(0xE1);
    lcd_write_data(0xF0);
    lcd_write_data(0x04);
    lcd_write_data(0x08);
    lcd_write_data(0x08);
    lcd_write_data(0x07);
    lcd_write_data(0x03);
    lcd_write_data(0x28);
    lcd_write_data(0x32);
    lcd_write_data(0x40);
    lcd_write_data(0x3B);
    lcd_write_data(0x19);
    lcd_write_data(0x18);
    lcd_write_data(0x2A);
    lcd_write_data(0x2E);

    lcd_write_command(0xE4);
    lcd_write_data(0x25);
    lcd_write_data(0x00);
    lcd_write_data(0x00);

    lcd_write_command(0x21);

    lcd_write_command(0x11);
    delay(120);

    lcd_write_command(0x29);
    delay(20);

    // Backlight on (matches sample setup's 100%)
    lcd_set_backlight(100);

    lcd_fill_screen(0x0000);
}
