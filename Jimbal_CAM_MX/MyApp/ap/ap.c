#include "ap.h"
#include "lcd.h"
#include "uart.h"
#include <stdio.h>

#define CAM_WIDTH  320
#define CAM_HEIGHT 240
#define LCD_WIDTH  240
#define LCD_HEIGHT 320

static uint16_t objX = 0, objY = 0;
static uint16_t oldX = 0, oldY = 0;

void apInit(void) {
    uartInit();         // RTOS 태스크 안에서 호출되므로 이제 정상 동작
    LCD_Init();
    LCD_FillScreen(WHITE);
    printf("System Start!\r\n");
}

void apMain(void) {
    // ORE 에러 클리어
    extern UART_HandleTypeDef huart6;
    //extern uint8_t rx_6data;  // ← 이 extern 제거, uart.c 내부에서만 사용
    if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart6);
        HAL_UART_Receive_IT(&huart6, (uint8_t*)0, 1); // 더미, 아래 참고
    }

    // 큐 모니터링
    static uint32_t last_time = 0;
    if (HAL_GetTick() - last_time > 1000) {
        printf("Queue: %lu\r\n", uartAvailable(0));
        last_time = HAL_GetTick();
    }

    // 패킷 파싱
    uartParsePacket();

    // 좌표 가져오기
    TrackData track = uartGetTrackData();

    if (track.detected) {
        int16_t targetX = (int16_t)((uint32_t)track.cx * LCD_WIDTH  / CAM_WIDTH)  - 10;
        int16_t targetY = (int16_t)((uint32_t)track.cy * LCD_HEIGHT / CAM_HEIGHT) - 10;

        if (targetX < 0) targetX = 0;
        if (targetY < 0) targetY = 0;
        if (targetX > LCD_WIDTH  - 20) targetX = LCD_WIDTH  - 20;
        if (targetY > LCD_HEIGHT - 20) targetY = LCD_HEIGHT - 20;

        objX = (uint16_t)targetX;
        objY = (uint16_t)targetY;

        printf("X:%d Y:%d\r\n", objX, objY);

        if (objX != oldX || objY != oldY) {
            LCD_DrawRect(oldX, oldY, 20, 20, WHITE);
            LCD_DrawRect(objX, objY, 20, 20, RED);
            oldX = objX;
            oldY = objY;
        }
    }

    osDelay(10);
}