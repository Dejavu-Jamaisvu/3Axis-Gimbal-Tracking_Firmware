#ifndef _LCD_H_
#define _LCD_H_
#include "hw_def.h"

#define RED     0xF800
#define BLUE    0x001F
#define BLACK   0x0000
#define WHITE   0xFFFF

void LCD_Init(void);
void LCD_SendCommand(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_FillScreen(uint16_t color);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
#endif