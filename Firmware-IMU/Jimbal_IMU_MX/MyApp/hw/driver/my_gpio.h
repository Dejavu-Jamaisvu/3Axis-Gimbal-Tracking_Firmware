#ifndef __HW_DRIVER_MY_GPIO_H__
#define __HW_DRIVER_MY_GPIO_H__

#include "hw_def.h"

//port number : 0 = A, 1 = B, 2 = C, 3 = D, 4 = E, 5 = F, 6 = G, 7 = H
void myGpioPinMode(uint8_t port, uint8_t pin, uint8_t mode);
bool gpioExtWrite(uint8_t port_idx, uint8_t pin_num, uint8_t state);
int8_t gpioExtRead(uint8_t port_idx, uint8_t pin_num);  // high low 상태 반환



#endif //__HW_DRIVER_MY_GPIO_H__)



