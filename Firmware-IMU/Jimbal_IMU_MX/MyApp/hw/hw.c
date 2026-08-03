//#include "driver/uart.h"

// #include "hw_def.h"
#include "hw.h"

void hwInit(void)
{
    ledInit();//보류
    uartInit();
    cliInit();
    // buttonInit();//보류
    // tempInit();//보류
    // uartOpen(0, 9600);

    // i2cInit();
    // mpu6050Init();
    servoInit(); 
    
    logInit();

}