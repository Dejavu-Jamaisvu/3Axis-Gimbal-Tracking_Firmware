#include "uart.h"
#include "cmsis_os2.h"

extern UART_HandleTypeDef huart2;
static osMessageQueueId_t uart_rx_q = NULL;
static osMutexId_t uart_tx_mutex = NULL;

static uint8_t rx_data; // 인터럽트로 받을 1바이트 변수

bool uartInit(void) {
    if(uart_rx_q == NULL){
        uart_rx_q = osMessageQueueNew(256, sizeof(uint8_t), NULL);
    }
    if(uart_tx_mutex == NULL){
        uart_tx_mutex = osMutexNew(NULL);
    }

    uartOpen(0, 9600); // 9600bps로 오픈
    
    // HAL 인터럽트 수신 시작
    HAL_UART_Receive_IT(&huart2, &rx_data, 1);
    return true;
}

// HAL 수신 완료 콜백 (솔님 코드 그대로)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart->Instance == USART2){
        if(uart_rx_q != NULL){
            osMessageQueuePut(uart_rx_q, &rx_data, 0, 0);
        }
        // 다음 바이트 수신을 위해 다시 호출
        HAL_UART_Receive_IT(&huart2, &rx_data, 1);
    }
}

uint32_t uartAvailable(uint8_t ch) {
    return (uart_rx_q != NULL) ? osMessageQueueGetCount(uart_rx_q) : 0;
}

uint8_t uartRead(uint8_t ch) {
    uint8_t ret = 0;
    if(uart_rx_q != NULL){
        osMessageQueueGet(uart_rx_q, &ret, NULL, 0);
    }
    return ret;
}

bool uartOpen(uint8_t ch, uint32_t baudrate) {
    huart2.Init.BaudRate = baudrate;
    if(HAL_UART_DeInit(&huart2) != HAL_OK) return false;
    if(HAL_UART_Init(&huart2) != HAL_OK) return false;
    return true;
}

uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len) {
    if(uart_tx_mutex == NULL) return 0;
    osMutexAcquire(uart_tx_mutex, osWaitForever);
    if(HAL_UART_Transmit(&huart2, p_data, len, 200) != HAL_OK) len = 0;
    osMutexRelease(uart_tx_mutex);
    return len;
}