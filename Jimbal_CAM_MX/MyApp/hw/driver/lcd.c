#include "lcd.h"
#include "my_spi.h"
#include "my_gpio.h"

void LCD_SendCommand(uint8_t cmd) {
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 0);
    gpioPinWrite(LCD_DC_PORT, LCD_DC_PIN, 0);
    spiWriteByte(cmd);
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 1);
}

void LCD_SendData(uint8_t data) {
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 0);
    gpioPinWrite(LCD_DC_PORT, LCD_DC_PIN, 1);
    spiWriteByte(data);
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 1);
}

void LCD_Init(void) {
    // 하드웨어 리셋
    gpioPinWrite(LCD_RST_PORT, LCD_RST_PIN, 0);
    HAL_Delay(50);
    gpioPinWrite(LCD_RST_PORT, LCD_RST_PIN, 1);
    HAL_Delay(120);

    LCD_SendCommand(0x01); // Software Reset
    HAL_Delay(100);

    // 전원 설정 (솔님 코드 설정 유지)
    LCD_SendCommand(0x3A); LCD_SendData(0x55); // 16-bit RGB
    LCD_SendCommand(0x36); LCD_SendData(0x48); // 방향 설정
    LCD_SendCommand(0x11); HAL_Delay(120);    // Sleep Out
    LCD_SendCommand(0x29); HAL_Delay(50);     // Display ON
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_SendCommand(0x2A);
    LCD_SendData(x0 >> 8); LCD_SendData(x0 & 0xFF);
    LCD_SendData(x1 >> 8); LCD_SendData(x1 & 0xFF);
    LCD_SendCommand(0x2B);
    LCD_SendData(y0 >> 8); LCD_SendData(y0 & 0xFF);
    LCD_SendData(y1 >> 8); LCD_SendData(y1 & 0xFF);
    LCD_SendCommand(0x2C);
}

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x + w > 240) w = 240 - x;
    if (y + h > 320) h = 320 - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 0);
    gpioPinWrite(LCD_DC_PORT, LCD_DC_PIN, 1);

    uint32_t total = (uint32_t)w * h;
    for (uint32_t i = 0; i < total; i++) {
        spiWrite16(color); // 고속 레지스터 전송
    }
    gpioPinWrite(LCD_CS_PORT, LCD_CS_PIN, 1);
}

void LCD_FillScreen(uint16_t color) {
    LCD_DrawRect(0, 0, 240, 320, color);
}