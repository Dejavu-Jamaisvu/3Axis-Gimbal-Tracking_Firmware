#include "ap.h"
#include "lcd.h"
#include "uart.h"

/* 카메라 및 LCD 해상도 정의 */
#define CAM_WIDTH  160
#define CAM_HEIGHT 120
#define LCD_WIDTH  240
#define LCD_HEIGHT 320

/* ESP32-CAM 패킷 프로토콜 정의 */
#define FRAME_STX  0x02
#define FRAME_ETX  0x03

/* 전역 변수: 좌표 및 상태 관리 */
uint16_t objX = 0, objY = 0;
uint16_t oldX = 0, oldY = 0;
bool isDetected = false;

/**
 * @brief 초기화 함수 (main.c의 osKernelStart 직전에 호출)
 */
void apInit(void) {
    uartInit();    // UART 설정 (Baudrate 115200 확인)
    LCD_Init();    // LCD 초기화
    
    // 배경을 파란색으로 채우고 시작
    LCD_FillScreen(BLUE);
}

/**
 * @brief 메인 루프 함수 (FreeRTOS DefaultTask 내에서 호출)
 */
void apMain(void) {
    // 1. UART2(ESP32-CAM 연결)에 패킷 한 줄(8바이트) 이상 쌓였는지 확인
if (uartAvailable(1) >= 8) { 
        if (uartRead(1) == FRAME_STX) {
            uint8_t cx_h = uartRead(1);
            uint8_t cx_l = uartRead(1);
            uint8_t cy_h = uartRead(1);
            uint8_t cy_l = uartRead(1);
            uint8_t det  = uartRead(1);
            uint8_t chk  = uartRead(1);
            uint8_t etx  = uartRead(1);

            // 3. 체크섬 계산 및 끝 바이트 검증 (데이터 무결성 확인)
            uint8_t calc_chk = cx_h ^ cx_l ^ cy_h ^ cy_l ^ det;

            if (etx == FRAME_ETX && chk == calc_chk) {
                isDetected = (det == 0x01);

                if (isDetected) {
                    // 4. 카메라 좌표 복원 (2바이트 병합)
                    uint16_t camX = (cx_h << 8) | cx_l;
                    uint16_t camY = (cy_h << 8) | cy_l;

                    // 5. 카메라 해상도(160x120) -> LCD 해상도(240x320) 매핑 계산
                    // 정수 연산 오차를 줄이기 위해 먼저 곱하고 나눕니다.
                    objX = (uint32_t)camX * LCD_WIDTH / CAM_WIDTH;
                    objY = (uint32_t)camY * LCD_HEIGHT / CAM_HEIGHT;

                    // 6. 좌표 제한 (화면 밖으로 나가는 것 방지)
                    if (objX > LCD_WIDTH - 20)  objX = LCD_WIDTH - 20;
                    if (objY > LCD_HEIGHT - 20) objY = LCD_HEIGHT - 20;

                    // 7. 이전 좌표와 다를 때만 새로 그리기 (잔상 제거 로직)
                    if (objX != oldX || objY != oldY) {
                        // 이전 위치는 배경색(BLUE)으로 지우기
                        LCD_DrawRect(oldX, oldY, 20, 20, BLUE);
                        
                        // 새 위치에 물체 표시(RED)
                        LCD_DrawRect(objX, objY, 20, 20, RED);
                        
                        // 현재 좌표를 이전 좌표로 저장
                        oldX = objX;
                        oldY = objY;
                    }
                } else {
                    // 물체가 감지되지 않을 때 (선택 사항: 기존 네모 지우기)
                    LCD_DrawRect(oldX, oldY, 20, 20, BLUE);
                }
            }
        }
    }
    
    // FreeRTOS 스케줄러가 다른 태스크를 돌 수 있도록 최소한의 지연시간 부여
    osDelay(1); 
}