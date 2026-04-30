#include "ap.h"
#include "lcd.h"
#include "spi.h"
#include <stdio.h>
#include <stdbool.h>

#define CAM_WIDTH   160
#define CAM_HEIGHT  120
#define LCD_WIDTH   240
#define LCD_HEIGHT  320
#define VIDEO_SIZE  (CAM_WIDTH * CAM_HEIGHT * 2)  
#define PACKET_SIZE (2 + VIDEO_SIZE + 5 + 2)      

extern SPI_HandleTypeDef hspi2;

// 1. 핑퐁 버퍼 선언 (A와 B 두 개의 그릇)
uint8_t spi_buf_A[PACKET_SIZE];
uint8_t spi_buf_B[PACKET_SIZE];

// 2. 포인터로 역할 분담
uint8_t *rx_buf = spi_buf_A;   // 수신용(ESP32 -> STM32) 그릇
uint8_t *draw_buf = spi_buf_B; // 출력용(STM32 -> LCD) 그릇

volatile bool dma_done = false;
static uint16_t oldX = 0, oldY = 0;

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI2) {
        dma_done = true;
    }
}

void apInit(void) {
    LCD_Init();
    LCD_FillScreen(BLACK);
    printf("Double Buffering SPI Slave Ready!\r\n");

    // 첫 수신 시작 (rx_buf인 A 그릇에 받기 시작)
    HAL_SPI_Receive_DMA(&hspi2, rx_buf, PACKET_SIZE);
}

void apMain(void) {
    // 과로(Overrun) 복구 로직
    if (__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_OVR)) {
        __HAL_SPI_CLEAR_OVRFLAG(&hspi2);
        HAL_SPI_Abort(&hspi2);
        dma_done = false;
        HAL_SPI_Receive_DMA(&hspi2, rx_buf, PACKET_SIZE);
        return;
    }

    if (!dma_done) {
        osDelay(1);
        return;
    }

    // ----------------------------------------------------
    // [마법의 구간] 수신 완료! 역할을 바꿉니다 (Pointer Swap)
    // ----------------------------------------------------
    dma_done = false;
    
    uint8_t *temp = rx_buf;
    rx_buf = draw_buf;     // 방금 전까지 그리던 빈 그릇을 수신용으로!
    draw_buf = temp;       // 꽉 찬 그릇을 그리기용으로!

    // CPU가 그림을 그리기 전에, 백그라운드에서는 이미 다음 프레임 수신 시작!! (병렬 처리)
    HAL_SPI_Receive_DMA(&hspi2, rx_buf, PACKET_SIZE);

    // ----------------------------------------------------
    // 이제부터 CPU는 꽉 찬 draw_buf만 여유롭게 그리면 됩니다.
    // ----------------------------------------------------
    if (draw_buf[0] != 0xAA || draw_buf[1] != 0xBB ||
        draw_buf[PACKET_SIZE - 2] != 0xCC ||
        draw_buf[PACKET_SIZE - 1] != 0xDD) {
        printf("Sync Error! Waiting for next frame...\r\n");
        return; 
    }

    // 화면 그리기 (뒤에서는 이미 다음 프레임이 수신되고 있음!)
    LCD_DrawImage(40, 100, CAM_WIDTH, CAM_HEIGHT, &draw_buf[2]);

    uint32_t idx = 2 + VIDEO_SIZE;
    int cx = (draw_buf[idx] << 8) | draw_buf[idx + 1];
    int cy = (draw_buf[idx + 2] << 8) | draw_buf[idx + 3];
    bool detected = (draw_buf[idx + 4] == 0x01);

    if (detected) {
        int16_t targetX = (int16_t)((uint32_t)cx * LCD_WIDTH  / CAM_WIDTH)  - 10;
        int16_t targetY = (int16_t)((uint32_t)cy * LCD_HEIGHT / CAM_HEIGHT) - 10;

        if (targetX < 0) targetX = 0;
        if (targetY < 0) targetY = 0;
        if (targetX > LCD_WIDTH  - 20) targetX = LCD_WIDTH  - 20;
        if (targetY > LCD_HEIGHT - 20) targetY = LCD_HEIGHT - 20;

        uint16_t objX = (uint16_t)targetX;
        uint16_t objY = (uint16_t)targetY;

        if (objX != oldX || objY != oldY) {
            LCD_DrawRect(oldX, oldY, 20, 20, BLACK);
            LCD_DrawRect(objX, objY, 20, 20, RED);
            oldX = objX;
            oldY = objY;
        }
    }
}