#include "usart.h" // Your USART6 header
#include "string.h"

// Assuming huart6 is initialized for PA11/PA12 at 115200 baud
void ESP8266_FetchFirmware() {
    char cmd[256];
    
    // 1. Set ESP8266 to Station Mode
    HAL_UART_Transmit(&huart6, (uint8_t*)"AT+CWMODE=1\r\n", 13, 1000);
    HAL_Delay(1000);

    // 2. Connect to Wi-Fi (Replace with your SSID/PASS)
    HAL_UART_Transmit(&huart6, (uint8_t*)"AT+CWJAP=\"kcci601\",\"@kcci601@\"\r\n", 32, 5000);
    HAL_Delay(5000);

    // 3. Configure SSL Buffer Size for HTTPS
    HAL_UART_Transmit(&huart6, (uint8_t*)"AT+CIPSSLSIZE=4096\r\n", 20, 1000);
    HAL_Delay(500);

    // 4. Start SSL Connection to AWS S3
    sprintf(cmd, "AT+CIPSTART=\"SSL\",\"gimbal-s3-imu-elf-file.s3.amazonaws.com\",443\r\n");
    HAL_UART_Transmit(&huart6, (uint8_t*)cmd, strlen(cmd), 5000);
    HAL_Delay(2000);

    // 5. Send HTTP GET Request
    char get_req[] = "GET /firmware.bin HTTP/1.1\r\nHost: gimbal-s3-imu-elf-file.s3.amazonaws.com\r\nConnection: close\r\n\r\n";
    sprintf(cmd, "AT+CIPSEND=%d\r\n", strlen(get_req));
    
    HAL_UART_Transmit(&huart6, (uint8_t*)cmd, strlen(cmd), 1000);
    HAL_Delay(500);
    HAL_UART_Transmit(&huart6, (uint8_t*)get_req, strlen(get_req), 2000);
    
    // At this point, you must use HAL_UART_Receive_IT or DMA to catch the incoming data.
}