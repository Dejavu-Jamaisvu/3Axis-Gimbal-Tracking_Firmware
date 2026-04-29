#include "ap.h"
#include "lcd.h"
#include "spi.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define CAM_WIDTH   160
#define CAM_HEIGHT  120
#define LCD_WIDTH   240
#define LCD_HEIGHT  320
#define VIDEO_SIZE  (CAM_WIDTH * CAM_HEIGHT * 2)  // 38,400
#define PACKET_SIZE (2 + VIDEO_SIZE + 5 + 2)      // 38,409

extern SPI_HandleTypeDef hspi2;

// DMA 수신 버퍼 - 전역으로 반드시 선언
uint8_t spi_rx_buf[PACKET_SIZE];

// DMA 완료 플래그
volatile bool dma_done = false;

static uint16_t oldX = 0, oldY = 0;

// ── DMA 수신 완료 콜백 ──────────────────────────────────
// SPI 수신이 끝나면 자동으로 여기가 호출됨
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI2) {
        dma_done = true;
    }
}

void apInit(void) {
    LCD_Init();
    LCD_FillScreen(BLACK);
    printf("DMA SPI Slave Ready!\r\n");

    // 첫 번째 DMA 수신 시작
    HAL_SPI_Receive_DMA(&hspi2, spi_rx_buf, PACKET_SIZE);
}

void apMain(void) {
    // DMA 완료됐는지 체크 (블로킹 없음)
    if (!dma_done) {
        osDelay(1);  // 아직 안 왔으면 1ms 대기
        return;
    }

    // 플래그 즉시 리셋 + 다음 DMA 수신 바로 시작
    dma_done = false;
    HAL_SPI_Receive_DMA(&hspi2, spi_rx_buf, PACKET_SIZE);

    // 헤더/푸터 검증
    if (spi_rx_buf[0] != 0xAA || spi_rx_buf[1] != 0xBB ||
        spi_rx_buf[PACKET_SIZE - 2] != 0xCC ||
        spi_rx_buf[PACKET_SIZE - 1] != 0xDD) {
        printf("Sync Error!\r\n");
        return;
    }

    // 영상 데이터 → LCD 출력
    LCD_DrawImage(40, 100, CAM_WIDTH, CAM_HEIGHT, &spi_rx_buf[2]);

    // 좌표 추출
    uint32_t idx = 2 + VIDEO_SIZE;
    int cx = (spi_rx_buf[idx] << 8) | spi_rx_buf[idx + 1];
    int cy = (spi_rx_buf[idx + 2] << 8) | spi_rx_buf[idx + 3];
    bool detected = (spi_rx_buf[idx + 4] == 0x01);

    // 빨간 네모 표시
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

    printf("OK! Detected:%s X:%d Y:%d\r\n", detected ? "YES" : "NO", cx, cy);
}