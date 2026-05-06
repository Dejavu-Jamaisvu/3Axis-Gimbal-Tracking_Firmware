#include "uart.h"
#include "usart.h"

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart6;

static osMessageQueueId_t uart_rx_q    = NULL;
static osMutexId_t        uart_tx_mutex = NULL;
static osMutexId_t        track_mutex   = NULL;

//static uint8_t rx_data;
static uint8_t rx_6data;  // static 유지, ap.c에서 extern 제거

static TrackData g_track = {0};

bool uartInit(void) {
    if (uart_rx_q == NULL)
        uart_rx_q = osMessageQueueNew(256, sizeof(uint8_t), NULL);
    if (uart_tx_mutex == NULL)
        uart_tx_mutex = osMutexNew(NULL);
    if (track_mutex == NULL)
        track_mutex = osMutexNew(NULL);

    // huart6(ESP32 수신용) 인터럽트 시작
    //HAL_UART_Receive_IT(&huart6, &rx_6data, 1);
    return true;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        if (uart_rx_q != NULL)
            osMessageQueuePut(uart_rx_q, &rx_6data, 0, 0);
        HAL_UART_Receive_IT(&huart6, &rx_6data, 1);
    }
}

// ESP32 패킷: [STX][cx_H][cx_L][cy_H][cy_L][det][ETX] = 7바이트
void uartParsePacket(void) {
    static uint8_t buf[7];
    static uint8_t idx = 0;

    while (osMessageQueueGetCount(uart_rx_q) > 0) {
        uint8_t byte = 0;
        osMessageQueueGet(uart_rx_q, &byte, NULL, 0);

        if (idx == 0 && byte != 0x02) continue; // STX 아니면 버림

        buf[idx++] = byte;

        if (idx == 7) {
            idx = 0;
            if (buf[6] != 0x03) continue; // ETX 확인

            int  cx       = (buf[1] << 8) | buf[2];
            int  cy       = (buf[3] << 8) | buf[4];
            bool detected = (buf[5] == 0x01);

            osMutexAcquire(track_mutex, osWaitForever);
            g_track.cx       = cx;
            g_track.cy       = cy;
            g_track.detected = detected;
            osMutexRelease(track_mutex);
        }
    }
}

TrackData uartGetTrackData(void) {
    TrackData out;
    osMutexAcquire(track_mutex, osWaitForever);
    out = g_track;
    osMutexRelease(track_mutex);
    return out;
}

uint32_t uartAvailable(uint8_t ch) {
    return (uart_rx_q != NULL) ? osMessageQueueGetCount(uart_rx_q) : 0;
}

uint8_t uartRead(uint8_t ch) {
    uint8_t ret = 0;
    if (uart_rx_q != NULL)
        osMessageQueueGet(uart_rx_q, &ret, NULL, 0);
    return ret;
}

bool uartOpen(uint8_t ch, uint32_t baudrate) {
    return true; // CubeMX에서 이미 115200으로 설정됨
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len) {
    if (uart_tx_mutex == NULL) return 0;
    osMutexAcquire(uart_tx_mutex, osWaitForever);
    HAL_UART_Transmit(&huart2, p_data, len, 200);
    osMutexRelease(uart_tx_mutex);
    return len;
}