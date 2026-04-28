#ifndef __HW_DRIVER_UART_H_ 
#define __HW_DRIVER_UART_H_

#include "hw_def.h"

bool     uartInit(void);
bool     uartOpen(uint8_t ch, uint32_t baudrate);
uint32_t uartAvailable(uint8_t ch);
uint8_t  uartRead(uint8_t ch);
uint32_t uartWrite(uint8_t ch, uint8_t *p_data, uint32_t len);

#endif