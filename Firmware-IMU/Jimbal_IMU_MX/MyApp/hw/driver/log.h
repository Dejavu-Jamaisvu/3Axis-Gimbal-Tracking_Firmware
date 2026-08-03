#ifndef __HW_DRIVER_LOG_H__
#define __HW_DRIVER_LOG_H__

#include "hw_def.h" // def.h, main.h, cmsis_os2.h, adc.h, dma.h, gpio.h 포함
#include "log_def.h"
#include "cli.h"
#include "uart.h"

void cliLog(uint8_t argc, char **argv);
bool logInit(void);
void logSetLevel(uint8_t level);
uint8_t logGetRuntimeLevel(void);



#endif //__HW_DRIVER_LOG_H__
