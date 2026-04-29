#include "lcd.h"
#include "spi.h"  // MX_SPI1_Init에서 생성된 huart1 등을 사용하기 위해

extern SPI_HandleTypeDef hspi1;

// 1. 명령어 전송 (HAL 방식)
// 테스트용 하이브리드 SendCommand
void LCD_SendCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET); // Command
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET); // CS Low
    
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
    // HAL 대신 레지스터 함수 사용
    //spiWriteByte(cmd); 
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);   // CS High
}

// 2. 데이터 전송 (HAL 방식)
void LCD_SendData(uint8_t data) {
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);   // Data
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET); // CS Low
    
    HAL_SPI_Transmit(&hspi1, &data, 1, 10); // HAL SPI 전송
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);   // CS High
}

void LCD_Init(void) {
    // 3. 하드웨어 리셋 (HAL 방식)
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(150);

    LCD_SendCommand(0x01); // Software Reset
    HAL_Delay(150);

    LCD_SendCommand(0x3A); LCD_SendData(0x55); // 16-bit RGB
    LCD_SendCommand(0x36); LCD_SendData(0x48); // 방향 설정
    LCD_SendCommand(0x11); HAL_Delay(150);     // Sleep Out
    LCD_SendCommand(0x29); HAL_Delay(100);     // Display ON
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_SendCommand(0x2A);
    uint8_t x_data[] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);

    LCD_SendCommand(0x2B);
    uint8_t y_data[] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);

    LCD_SendCommand(0x2C); // RAM Write 시작 준비
}

void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x + w > 240) w = 240 - x;
    if (y + h > 320) h = 320 - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);

    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);

    uint8_t color_buf[2] = {color >> 8, color & 0xFF};
    uint32_t total = (uint32_t)w * h;

    for (uint32_t i = 0; i < total; i++) {
        HAL_SPI_Transmit(&hspi1, color_buf, 2, 10); // 픽셀 하나씩 HAL로 전송
    }

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void LCD_FillScreen(uint16_t color) {
    LCD_DrawRect(0, 0, 240, 320, color);
}

void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data) {
    // 1. 영상을 그릴 네모난 영역(Window) 설정
    LCD_SetWindow(x, y, x + w - 1, y + h - 1);
    
    // 2. LCD에 데이터를 보낼 준비 (Data 모드, CS Low)
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);   
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET); 
    
    // 3. SPI1을 통해 배열에 담긴 영상 데이터를 전부 전송 (w * h * 2 바이트)
    HAL_SPI_Transmit(&hspi1, data, w * h * 2, 1000);
    
    // 4. 전송 완료 후 CS High
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);   
}