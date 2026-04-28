#include "ap.h"
#include "lcd.h"
#include "uart.h"

// 변수들을 일로 옮겼습니다.
uint16_t targetX = 110;
uint16_t targetY = 150;
uint16_t boxSize = 20;

void apInit(void) {
    uartInit();
    LCD_Init();
    LCD_FillScreen(BLUE);
}

void apMain(void) {
    if (uartAvailable(0) > 0) 
    {
        uint8_t rxChar = uartRead(0); 
        
        uint16_t oldX = targetX;
        uint16_t oldY = targetY;

        // 키보드 입력(WASD) 조종
        if      (rxChar == 'w') targetY -= 3;
        else if (rxChar == 's') targetY += 3;
        else if (rxChar == 'a') targetX -= 3;
        else if (rxChar == 'd') targetX += 3;

        // 경계 체크
        if (targetX < 1) targetX = 1;
        if (targetX > 219) targetX = 219;
        if (targetY < 1) targetY = 1;
        if (targetY > 299) targetY = 299;

        // 화면 업데이트
        if (oldX != targetX || oldY != targetY) {
            LCD_DrawRect(oldX, oldY, boxSize, boxSize, BLUE);
            LCD_DrawRect(targetX, targetY, boxSize, boxSize, RED);
        }
    }
    // RTOS 딜레이
    osDelay(1);
}