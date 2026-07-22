#include "uart.h"
#include "usart.h"

extern UART_HandleTypeDef huart2;  // PC 시리얼 출력
extern UART_HandleTypeDef huart6;  // Board B 송신

static osMutexId_t uart2_tx_mutex = NULL;
static osMutexId_t uart6_tx_mutex = NULL;

bool uartInit(void) {
    if (uart2_tx_mutex == NULL)
        uart2_tx_mutex = osMutexNew(NULL);
    if (uart6_tx_mutex == NULL)
        uart6_tx_mutex = osMutexNew(NULL);

    // huart6 수신 인터럽트 제거
    // ESP32는 SPI로 받고, huart6는 송신 전용

    printf("[UART] 초기화 완료\r\n");
    return true;
}

// Board B로 좌표 전송 (UART6)
void uartSendTrackData(int cx, int cy, bool detected) {
    uint8_t packet[7];
    packet[0] = 0x02;                    // STX
    packet[1] = (cx >> 8) & 0xFF;        // CX High MSB먼저 보내기
    packet[2] =  cx & 0xFF;              // CX Low
    packet[3] = (cy >> 8) & 0xFF;        // CY High
    packet[4] =  cy & 0xFF;              // CY Low
    packet[5] =  detected ? 0x01 : 0x00; // 감지여부
    packet[6] = 0x03;                    // ETX

    if (uart6_tx_mutex == NULL) return;
    osMutexAcquire(uart6_tx_mutex, osWaitForever);
    HAL_UART_Transmit(&huart6, packet, 7, 100);
    osMutexRelease(uart6_tx_mutex);
}

// PC 시리얼 출력 (UART2)
uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len) {
    if (uart2_tx_mutex == NULL) return 0;
    osMutexAcquire(uart2_tx_mutex, osWaitForever);
    HAL_UART_Transmit(&huart2, p_data, len, 200);
    osMutexRelease(uart2_tx_mutex);
    return len;
}