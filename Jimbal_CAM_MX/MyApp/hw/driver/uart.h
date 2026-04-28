#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

// ESP32에서 받은 좌표 데이터
typedef struct {
    int  cx;
    int  cy;
    bool detected;
} TrackData;

bool     uartInit(void);
bool     uartOpen(uint8_t ch, uint32_t baudrate);
uint32_t uartAvailable(uint8_t ch);
uint8_t  uartRead(uint8_t ch);
uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len);

void      uartParsePacket(void);   // 매 루프마다 호출
TrackData uartGetTrackData(void);  // 최신 좌표 반환