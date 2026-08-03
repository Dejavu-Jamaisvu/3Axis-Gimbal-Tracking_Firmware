
#include "uart.h"

#include "cmsis_os2.h"

// #include <cstddef>
// #include "cmsis_os.h"
// #include "cmsis_os2.h"
// #include "stm32f4xx_hal_uart.h"
// #include <stdint.h>
// #include <stdio.h>
// #include <string.h>

extern UART_HandleTypeDef huart1; // ESP32용 추가
extern UART_HandleTypeDef huart2; // PC 디버깅용

// static osMessageQueueId_t uart_rx_q = NULL;
static osMessageQueueId_t uart_rx_q[2] = {NULL, NULL};


static osMutexId_t uart_tx_mutex = NULL;

#define TIMEOUT 100

#define UART_RX_BUF_LENGTH 256

// static uint8_t rx_buf[UART_RX_BUF_LENGTH];
// static uint32_t rx_buf_head = 0;
// static uint32_t rx_buf_tail = 0;
// static uint8_t rx_data = 0;

static uint8_t rx_data[2] = {0, 0};

bool uartInit(void)
{
    // if (uart_rx_q == NULL) {
    //     uart_rx_q = osMessageQueueNew(UART_RX_BUF_LENGTH, sizeof(uint8_t), NULL);
    // }

    // 채널 0 (PC용 USART2) 초기화
    if (uart_rx_q[0] == NULL) {
        uart_rx_q[0] = osMessageQueueNew(UART_RX_BUF_LENGTH, sizeof(uint8_t), NULL);
    }
    // 채널 1 (ESP32용 USART1) 초기화
    if (uart_rx_q[1] == NULL) {
        uart_rx_q[1] = osMessageQueueNew(UART_RX_BUF_LENGTH, sizeof(uint8_t), NULL);
    }


    if (uart_tx_mutex == NULL) {
        uart_tx_mutex = osMutexNew(NULL);
    }

    // bool ret = uartOpen(0, 1152000); // 9600 -> 프로젝트 시 115200으로 설정
    uartOpen(0, 115200); // USART2// USART2
    uartOpen(1, 115200); // USART1 (ESP32)

    
    // HAL_UART_Receive_IT(&huart2, &rx_data, 1);
    HAL_UART_Receive_IT(&huart2, &rx_data[0], 1);
    HAL_UART_Receive_IT(&huart1, &rx_data[1], 1); // USART1 수신 시작

    // return uartOpen(0, 115200);
    // return ret;
    return true;

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

    // if (huart->Instance == USART2) {
    //     // rx_buf[rx_buf_head] = rx_data;
    //     // rx_buf_head = (rx_buf_head + 1 ) % UART_RX_BUF_LENGTH;

    //     if (uart_rx_q != NULL) {
    //         osMessageQueuePut(uart_rx_q, &rx_data, 0, 0);
    //     }
    //     HAL_UART_Receive_IT(&huart2, &rx_data, 1);
    // }

    if (huart->Instance == USART2) {
        if (uart_rx_q[0] != NULL) osMessageQueuePut(uart_rx_q[0], &rx_data[0], 0, 0);
        HAL_UART_Receive_IT(&huart2, &rx_data[0], 1);
    }
    else if (huart->Instance == USART1) { // 채널 1 추가
        if (uart_rx_q[1] != NULL) osMessageQueuePut(uart_rx_q[1], &rx_data[1], 0, 0);
        HAL_UART_Receive_IT(&huart1, &rx_data[1], 1);
    }
}
uint32_t uartAvailable(uint8_t ch)
{
    // uint32_t ret = 0; //초기화 //쓰레기값 방지

    // if(rx_buf_head != rx_buf_tail){
    //     if(rx_buf_head > rx_buf_tail){
    //         ret = rx_buf_head - rx_buf_tail;
    //     }else{
    //         ret = UART_RX_BUF_LENGTH - (rx_buf_tail - rx_buf_head);
    //     }
    // }
    // return ret;

    // if (ch == 0 && uart_rx_q != NULL) {
    //     return osMessageQueueGetCount(uart_rx_q);
    // }

    // ch 인덱스를 사용하여 큐를 체크합니다.
    if (ch < 2 && uart_rx_q[ch] != NULL) {
        return osMessageQueueGetCount(uart_rx_q[ch]);
    }
    return 0;
}
uint8_t uartRead(uint8_t ch)
{
    uint8_t ret = 0;

    // if (rx_buf_head != rx_buf_tail) {
    //     ret = rx_buf[rx_buf_tail];
    //     rx_buf_tail = (rx_buf_tail + 1) % UART_RX_BUF_LENGTH;
    // } ////

    // if (ch == 0 && uart_rx_q != NULL) {
    //     osMessageQueueGet(uart_rx_q, &ret, NULL, 0);
    // }

    if (ch < 2 && uart_rx_q[ch] != NULL) {
        osMessageQueueGet(uart_rx_q[ch], &ret, NULL, 0);
    }
    return ret;
}

bool uartReadBlock(uint8_t ch, uint8_t *p_data, uint32_t timeout)
{
    // if (ch == 0 && uart_rx_q != NULL) {

    //     if (osMessageQueueGet(uart_rx_q, p_data, NULL, timeout) == osOK)

    //         return true;
    // }
    if (ch < 2 && uart_rx_q[ch] != NULL) {
        if (osMessageQueueGet(uart_rx_q[ch], p_data, NULL, timeout) == osOK)
            return true;
    }
    return false;
}

bool uartOpen(uint8_t ch, uint32_t baudrate)
{
    // if (huart2.Init.BaudRate != baudrate)
    //     huart2.Init.BaudRate = baudrate;

    UART_HandleTypeDef *p_huart;

    if (ch == 0) p_huart = &huart2;
    else        p_huart = &huart1;

    p_huart->Init.BaudRate = baudrate;
    
    // huart2 고정이 아니라 p_huart를 사용해야 함
    if (HAL_UART_DeInit(p_huart) != HAL_OK) return false;
    if (HAL_UART_Init(p_huart) != HAL_OK) return false;

    return true;
}

bool uartClose(uint8_t ch)
{

    return true;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len)
{
    if (uart_tx_mutex == NULL)
        return 0;

    UART_HandleTypeDef *p_huart = (ch == 0) ? &huart2 : &huart1;
    osMutexAcquire(uart_tx_mutex, osWaitForever);

    // if (HAL_UART_Transmit(&huart2, p_data, len, TIMEOUT) == HAL_OK) {
    //     osDelay(1);
    //     osMutexRelease(uart_tx_mutex);
    // } else {
    //     osDelay(1);
    // }

    // if (HAL_UART_Transmit(&huart2, p_data, len, TIMEOUT) == HAL_OK) {
    // } else {
    //     len = 0;
    // }

    if (HAL_UART_Transmit(p_huart, p_data, len, TIMEOUT) != HAL_OK) {
        len = 0;
    }
    osMutexRelease(uart_tx_mutex);

    return len;
}
uint32_t uartPrintf(uint8_t ch, const char *fmt, ...)
{

    char buf[128];
    uint32_t len;
    va_list args;

    va_start(args, fmt);
    len = vsnprintf(buf, 128, fmt, args);
    va_end(args);

    return uartWrite(ch, (uint8_t *)buf, len);
}