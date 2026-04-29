#include "ap.h"
#include "lcd.h"
#include "spi.h"
#include <stdio.h>
#include <stdbool.h>

/* 카메라 및 LCD 해상도 정의 */
#define CAM_WIDTH  160  // ESP32는 반드시 QQVGA로 설정되어 있어야 함!
#define CAM_HEIGHT 120
#define LCD_WIDTH  240
#define LCD_HEIGHT 320

/* ESP32가 보내는 전체 패킷 크기 계산 */
// 헤더(2) + 영상데이터(38400) + 좌표(5) + 푸터(2) = 38,409 바이트
#define VIDEO_SIZE   (CAM_WIDTH * CAM_HEIGHT * 2)
#define PACKET_SIZE  (2 + VIDEO_SIZE + 5 + 2)

// 엄청나게 큰 배열이므로 반드시 전역 변수로 선언해야 메모리가 터지지 않습니다.
uint8_t spi_rx_buf[PACKET_SIZE];

extern SPI_HandleTypeDef hspi2;

static uint16_t oldX = 0, oldY = 0;

void apInit(void) {
    LCD_Init();
    LCD_FillScreen(BLACK); // 영상에 집중하기 위해 배경은 검은색
    printf("SPI Slave Mode Ready! Waiting for ESP32 Master...\r\n");
}

void apMain(void) {
    // 1. ESP32(Master)가 클럭을 주며 데이터를 쏠 때까지 대기하며 통째로 수신
    HAL_StatusTypeDef res = HAL_SPI_Receive(&hspi2, spi_rx_buf, PACKET_SIZE, 1000);

    if (res == HAL_OK) {
        // 2. 패킷 앞뒤의 헤더/푸터가 정확한지 확인 (통신이 엇갈리지 않았는지 검증)
        if (spi_rx_buf[0] == 0xAA && spi_rx_buf[1] == 0xBB && 
            spi_rx_buf[PACKET_SIZE - 2] == 0xCC && spi_rx_buf[PACKET_SIZE - 1] == 0xDD) {
            
            // 3. LCD 정중앙(X:40, Y:100)에 영상 데이터 띄우기
            LCD_DrawImage(40, 100, CAM_WIDTH, CAM_HEIGHT, &spi_rx_buf[2]);

            // 4. 좌표 데이터 추출 (영상 데이터 38,400바이트 바로 뒤에 있음)
            uint32_t track_idx = 2 + VIDEO_SIZE;
            int cx = (spi_rx_buf[track_idx] << 8) | spi_rx_buf[track_idx + 1];
            int cy = (spi_rx_buf[track_idx + 2] << 8) | spi_rx_buf[track_idx + 3];
            bool detected = (spi_rx_buf[track_idx + 4] == 0x01);

            // 5. 물체가 감지되었으면 빨간 네모 그리기
            if (detected) {
                // 수신된 카메라 좌표(160x120)를 LCD(240x320) 스케일에 맞게 변환
                int16_t targetX = (int16_t)((uint32_t)cx * LCD_WIDTH  / CAM_WIDTH)  - 10;
                int16_t targetY = (int16_t)((uint32_t)cy * LCD_HEIGHT / CAM_HEIGHT) - 10;

                // 화면 밖으로 네모가 나가지 않도록 좌표 제한
                if (targetX < 0) targetX = 0;
                if (targetY < 0) targetY = 0;
                if (targetX > LCD_WIDTH  - 20) targetX = LCD_WIDTH  - 20;
                if (targetY > LCD_HEIGHT - 20) targetY = LCD_HEIGHT - 20;

                uint16_t objX = (uint16_t)targetX;
                uint16_t objY = (uint16_t)targetY;

                // 물체가 이동했을 때만 화면 업데이트 (잔상 제거)
                if (objX != oldX || objY != oldY) {
                    LCD_DrawRect(oldX, oldY, 20, 20, BLACK); // 이전 자리 지우기
                    LCD_DrawRect(objX, objY, 20, 20, RED);   // 새 위치에 빨간 네모
                    oldX = objX;
                    oldY = objY;
                }
            }
            // 테라텀으로 실시간 감지 상태 출력
            printf("Frame OK! Target Detected: %s (X:%d, Y:%d)\r\n", detected ? "YES" : "NO", cx, cy);
            
        } else {
            // 헤더/푸터가 안 맞으면 노이즈가 낀 것이므로 통신 상태 강제 리셋
            printf("SPI Sync Error! Resetting SPI...\r\n");
            HAL_SPI_Abort(&hspi2);
        }
    } else {
        // ESP32가 데이터를 안 보내고 있을 때
        printf("Waiting for ESP32 Data...\r\n");
    }

    // FreeRTOS 스케줄러 딜레이
    osDelay(10); 
}