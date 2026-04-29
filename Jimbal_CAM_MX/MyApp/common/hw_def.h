#ifndef __HW_HW_DEF_H_
#define __HW_HW_DEF_H_

#include "main.h"
#include "stm32f411xe.h"
#include "def.h"
#include "cmsis_os2.h"

// LCD 핀 설정 (솔님의 실제 연결: GPIOA)
#define LCD_CS_PORT     GPIOA
#define LCD_CS_PIN      GPIO_PIN_4

#define LCD_DC_PORT     GPIOA
#define LCD_DC_PIN      GPIO_PIN_8

#define LCD_RST_PORT    GPIOA
#define LCD_RST_PIN     GPIO_PIN_9

#define LCD_WIDTH 240
#define LCD_HEIGHT 320

#endif // __HW_HW_DEF_H_    