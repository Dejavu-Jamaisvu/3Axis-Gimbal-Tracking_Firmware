#ifndef _UART_H_
#define _UART_H_

#include "hw_def.h"
#include <stdbool.h>
#include <stdint.h>

bool     uartInit(void);
void     uartSendTrackData(int cx, int cy, bool detected); // Board B 송신
uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len); // PC 출력

#endif