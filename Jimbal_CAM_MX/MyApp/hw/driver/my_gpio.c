#include "my_gpio.h"

void gpioWrite(GPIO_TypeDef *port, uint16_t pin, uint8_t state) {
    gpioPinWrite(port, pin, state);
}