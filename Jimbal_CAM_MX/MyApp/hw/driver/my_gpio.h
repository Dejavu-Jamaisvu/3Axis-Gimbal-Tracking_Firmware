#ifndef MY_GPIO_H_
#define MY_GPIO_H_
#include "hw_def.h"

// 인라인 함수로 선언하여 함수 호출 오버헤드 제거
static inline void gpioPinWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t state) {
    if (state) port->BSRR = pin;           // Set (High)
    else       port->BSRR = (uint32_t)pin << 16; // Reset (Low)
}

void gpioWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t state);
#endif