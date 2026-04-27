#include "lcd.h"

/* * CubeMX가 main.c에서 자동으로 만든 SPI 핸들을 여기서도 쓰겠다고 선언합니다.
 * 만약 SPI2를 쓰신다면 hspi2로 변경하세요.
 */
extern SPI_HandleTypeDef hspi1; 

/* 명령어 전송 함수 (DC 핀을 Low로 내리고 전송) */
void LCD_SendCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);  // CS Low (통신 시작)
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET);  // DC Low (명령어 모드)
    
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);            // 1바이트 전송
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);    // CS High (통신 종료)
}

/* 데이터 전송 함수 (DC 핀을 High로 올리고 전송) */
void LCD_SendData(uint8_t data) {
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);  // CS Low
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);    // DC High (데이터 모드)
    
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);           // 1바이트 전송
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);    // CS High
}

void LCD_Init(void) {
    /* 1. 하드웨어 리셋 (데이터시트 권장 딜레이) */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120); // 리셋 후 안정화 대기

    /* 2. 소프트웨어 리셋 */
    LCD_SendCommand(0x01);
    HAL_Delay(100);

    /* 3. 전원 및 VCOM (전압) 설정 (백화현상 해결의 핵심!) */
    LCD_SendCommand(0xC0); // Power Control 1
    LCD_SendData(0x23);

    LCD_SendCommand(0xC1); // Power Control 2
    LCD_SendData(0x10);

    LCD_SendCommand(0xC5); // VCOM Control 1
    LCD_SendData(0x3E);
    LCD_SendData(0x28);

    LCD_SendCommand(0xC7); // VCOM Control 2
    LCD_SendData(0x86);

    /* 4. 메모리 및 픽셀 포맷 설정 */
    LCD_SendCommand(0x36); // Memory Access Control (방향 설정)
    LCD_SendData(0x48);    // 0x48: 기본 세로 모드

    LCD_SendCommand(0x3A); // Pixel Format Set
    LCD_SendData(0x55);    // 0x55: 16-bit RGB (우리가 아까 만든 RED, BLUE 규격)

    /* 5. 프레임 레이트 및 디스플레이 제어 */
    LCD_SendCommand(0xB1); // Frame Rate Control
    LCD_SendData(0x00);
    LCD_SendData(0x18);

    LCD_SendCommand(0xB6); // Display Function Control
    LCD_SendData(0x08);
    LCD_SendData(0x82);
    LCD_SendData(0x27);

    /* 6. 절전 모드 해제 및 화면 켜기 */
    LCD_SendCommand(0x11); // Sleep Out
    HAL_Delay(120);        // 데이터시트 필수: Sleep Out 후 120ms 대기!

    LCD_SendCommand(0x29); // Display ON
    HAL_Delay(50);
}

/* 화면의 그림 그릴 영역(창문)을 설정하는 함수 */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    LCD_SendCommand(0x2A); // Column Address Set (X 좌표)
    LCD_SendData(x0 >> 8);
    LCD_SendData(x0 & 0xFF);
    LCD_SendData(x1 >> 8);
    LCD_SendData(x1 & 0xFF);

    LCD_SendCommand(0x2B); // Page Address Set (Y 좌표)
    LCD_SendData(y0 >> 8);
    LCD_SendData(y0 & 0xFF);
    LCD_SendData(y1 >> 8);
    LCD_SendData(y1 & 0xFF);

    LCD_SendCommand(0x2C); // Memory Write (이제부터 데이터 들어간다!)
}

/* 화면 전체를 특정 색상으로 덮는 함수 */
void LCD_FillScreen(uint16_t color) {
    uint32_t i;
    // 16비트 색상을 전송하기 위해 8비트 2개로 분할
    uint8_t data[2] = { color >> 8, color & 0xFF }; 

    // ILI9341 기본 해상도 240x320 전체 영역 잡기
    LCD_SetWindow(0, 0, 239, 319); 

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);  // CS Low
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);    // DC High (데이터 모드)

    // 240 * 320 = 76,800 픽셀만큼 색상 데이터 전송
    for(i = 0; i < 76800; i++) {
        HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
    }
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);    // CS High
}

// (x, y) 위치에 가로 w, 세로 h 크기의 사각형을 그리는 함수
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // 1. 화면 범위를 벗어나지 않도록 방어 코드
    if (x + w > 240) w = 240 - x;
    if (y + h > 320) h = 320 - y;

    // 2. 그릴 영역(Window) 설정
    LCD_SetWindow(x, y, x + w - 1, y + h - 1);

    // 3. 색상 데이터 준비
    uint8_t data[2] = { color >> 8, color & 0xFF };

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);

    // 4. 전체 면적(w * h)만큼 색상 밀어넣기
    uint32_t total_pixels = (uint32_t)w * h;
    for (uint32_t i = 0; i < total_pixels; i++) {
        HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
    }

    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= 240 || y >= 320) return; // 화면 범위를 벗어나면 무시
    
    LCD_SetWindow(x, y, x, y); // 점 하나 크기로 창 지정
    
    uint8_t data[2] = { color >> 8, color & 0xFF };
    
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET);
}