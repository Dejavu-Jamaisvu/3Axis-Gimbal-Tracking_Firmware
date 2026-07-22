#include "ap.h"
#include "lcd.h"
#include "spi.h"
#include "uart.h"
#include <stdio.h>
#include <stdbool.h>

#define CAM_WIDTH   160
#define CAM_HEIGHT  120
#define LCD_WIDTH   240
#define LCD_HEIGHT  320
#define VIDEO_SIZE  (CAM_WIDTH * CAM_HEIGHT * 2)
#define PACKET_SIZE (2 + VIDEO_SIZE + 5 + 2) // 총 38,409 바이트

extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi1;

// 1. 핑퐁 버퍼 선언 (A와 B 두 개의 그릇)
uint8_t spi_buf_A[PACKET_SIZE];
uint8_t spi_buf_B[PACKET_SIZE];

// 2. 포인터로 역할 분담
uint8_t *rx_buf = spi_buf_A;   // 수신용(ESP32 -> STM32) 그릇
uint8_t *draw_buf = spi_buf_B; // 출력용(STM32 -> LCD) 그릇

volatile bool dma_done = false;

// 진동벨(세마포어) 변수
osSemaphoreId_t spi1_sem = NULL;

// --------------------------------------------------
// [입력 고속도로] 카메라 데이터 수신 완료 알람 (SPI2)
// --------------------------------------------------
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI2) {
        dma_done = true; 
    }
}

// --------------------------------------------------
// [출력 고속도로] LCD 화면 그리기 완료 알람 (SPI1)
// --------------------------------------------------
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        // 화면 전송이 완전히 끝났으므로 CS 핀을 닫아줍니다 (HIGH)
        HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET); 

        // ✅ 진동벨은 화면 다 그렸을 때 여기서 울려야 합니다!
        if (spi1_sem != NULL) {
            osSemaphoreRelease(spi1_sem); 
        }
    }
}

void apInit(void) {
    LCD_Init();
    LCD_FillScreen(BLACK);
    uartInit();

    // 진동벨 생성
    spi1_sem = osSemaphoreNew(1, 0, NULL);

    printf("SPI Slave !\r\n");

    // 첫 수신 시작 (rx_buf인 A 그릇에 받기 시작)
    HAL_SPI_Receive_DMA(&hspi2, rx_buf, PACKET_SIZE);
}

void apMain(void) {
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

    // 화면 그리기 (시작위치 화면상 40,100) -> DMA 백그라운드 전송 시작
    LCD_DrawImage(40, 100, CAM_WIDTH, CAM_HEIGHT, &draw_buf[2]);

    // 여기서부터 좌표 추출 (변수 생성)
    uint32_t idx = 2 + VIDEO_SIZE;
    int cx = (draw_buf[idx] << 8) | draw_buf[idx + 1];
    int cy = (draw_buf[idx + 2] << 8) | draw_buf[idx + 3];
    bool detected = (draw_buf[idx + 4] == 0x01);

    // ✅ 변수가 다 만들어졌으니 이제 전송합니다!
    uartSendTrackData(cx, cy, detected);

    // ✅ 진동벨이 울릴 때까지 대기 (화면 그리기 끝날 때까지)
    if (osSemaphoreAcquire(spi1_sem, 50) == osOK) {
        
        // 진동벨이 울렸음 (화면 전송 무사히 완료!) -> 네모 그리기 시작
        if (detected) {
            int16_t targetX = 40 + cx - 10;
            int16_t targetY = 100 + cy - 10;

            if (targetX < 40) targetX = 40;
            if (targetY < 100) targetY = 100;
            if (targetX > 40 + CAM_WIDTH  - 20) targetX = 40 + CAM_WIDTH  - 20;
            if (targetY > 100 + CAM_HEIGHT - 20) targetY = 100 + CAM_HEIGHT - 20;

            LCD_DrawHollowRect(targetX, targetY, 20, 20, 2, RED);
        }
    } else {
        // 50ms가 지나도 진동벨이 안 울림 (통신 꼬임 등 에러 발생 시 비상 탈출)
        printf("[ERROR] LCD 화면 그리기 시간 초과!!\r\n");
        HAL_SPI_Abort(&hspi1);
        HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    }
}