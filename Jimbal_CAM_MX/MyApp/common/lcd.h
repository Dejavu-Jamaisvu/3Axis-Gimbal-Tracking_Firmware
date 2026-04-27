#ifndef _LCD_H_
#define _LCD_H_

#include "stm32f4xx_hal.h"

#define LCD_CS_PORT     GPIOA
#define LCD_CS_PIN      GPIO_PIN_4

#define LCD_DC_PORT     GPIOA
#define LCD_DC_PIN      GPIO_PIN_8

#define LCD_RST_PORT    GPIOA
#define LCD_RST_PIN     GPIO_PIN_9

typedef struct {
    const uint8_t* data;  // 폰트 데이터 배열의 주소
    uint8_t width;        // 가로 크기 (6)
    uint8_t height;       // 세로 크기 (8)
} LCD_Object;

void LCD_Init(void);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);

#define RED     0xF800
#define BLUE    0x001F
#define BLACK   0x0000
#define WHITE   0xFFFF

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_FillScreen(uint16_t color);

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
/* lcd.h */
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

#endif /* _LCD_H_ */