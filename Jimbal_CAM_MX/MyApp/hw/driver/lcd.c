#include "lcd.h"
#include "spi.h"

extern SPI_HandleTypeDef hspi1;

/* ── 내부 헬퍼 ─────────────────────────────────────────── */
static void _cs_low(void)  { HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_RESET); }
static void _cs_high(void) { HAL_GPIO_WritePin(LCD_CS_PORT,  LCD_CS_PIN,  GPIO_PIN_SET);   }
static void _dc_cmd(void)  { HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_RESET); }
static void _dc_data(void) { HAL_GPIO_WritePin(LCD_DC_PORT,  LCD_DC_PIN,  GPIO_PIN_SET);   }

static void _write_cmd(uint8_t cmd) {
    _dc_cmd();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 10);
}

static void _write_data(const uint8_t *buf, uint32_t len) {
    _dc_data();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, 500);
}

static void _write_data_byte(uint8_t d) {
    _write_data(&d, 1);
}

/* ── 공개 API (ap.c 등에서 쓰는 것들) ─────────────────── */
void LCD_SendCommand(uint8_t cmd) {
    _cs_low();
    _write_cmd(cmd);
    _cs_high();
}

void LCD_SendData(uint8_t data) {
    _cs_low();
    _write_data_byte(data);
    _cs_high();
}

/* ── ILI9341 초기화 ────────────────────────────────────── */
void LCD_Init(void) {
    // 하드웨어 리셋
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);

    _cs_low();  // 초기화 전체를 CS LOW 유지

    _write_cmd(0x01); HAL_Delay(120);   // Software Reset

    _write_cmd(0xCB);                   // Power Control A
    _write_data_byte(0x39); _write_data_byte(0x2C);
    _write_data_byte(0x00); _write_data_byte(0x34);
    _write_data_byte(0x02);

    _write_cmd(0xCF);                   // Power Control B
    _write_data_byte(0x00); _write_data_byte(0xC1);
    _write_data_byte(0x30);

    _write_cmd(0xE8);                   // Driver Timing A
    _write_data_byte(0x85); _write_data_byte(0x00);
    _write_data_byte(0x78);

    _write_cmd(0xEA);                   // Driver Timing B
    _write_data_byte(0x00); _write_data_byte(0x00);

    _write_cmd(0xED);                   // Power On Sequence
    _write_data_byte(0x64); _write_data_byte(0x03);
    _write_data_byte(0x12); _write_data_byte(0x81);

    _write_cmd(0xF7);                   // Pump Ratio
    _write_data_byte(0x20);

    _write_cmd(0xC0); _write_data_byte(0x23);  // Power 1
    _write_cmd(0xC1); _write_data_byte(0x10);  // Power 2

    _write_cmd(0xC5);                   // VCOM 1
    _write_data_byte(0x3E); _write_data_byte(0x28);

    _write_cmd(0xC7); _write_data_byte(0x86); // VCOM 2

    // MADCTL: 방향 설정
    // 0x48 = 세로(Portrait), 0x28 = 가로(Landscape)
    _write_cmd(0x36); _write_data_byte(0x48);

    _write_cmd(0x3A); _write_data_byte(0x55);  // 픽셀 포맷 16bit

    _write_cmd(0xB1);                   // Frame Rate
    _write_data_byte(0x00); _write_data_byte(0x18);

    _write_cmd(0xB6);                   // Display Function
    _write_data_byte(0x08); _write_data_byte(0x82);
    _write_data_byte(0x27);

    _write_cmd(0xF2); _write_data_byte(0x00); // 3Gamma off

    _write_cmd(0x26); _write_data_byte(0x01); // Gamma curve

    _write_cmd(0xE0);                   // Positive Gamma
    _write_data_byte(0x0F); _write_data_byte(0x31);
    _write_data_byte(0x2B); _write_data_byte(0x0C);
    _write_data_byte(0x0E); _write_data_byte(0x08);
    _write_data_byte(0x4E); _write_data_byte(0xF1);
    _write_data_byte(0x37); _write_data_byte(0x07);
    _write_data_byte(0x10); _write_data_byte(0x03);
    _write_data_byte(0x0E); _write_data_byte(0x09);
    _write_data_byte(0x00);

    _write_cmd(0xE1);                   // Negative Gamma
    _write_data_byte(0x00); _write_data_byte(0x0E);
    _write_data_byte(0x14); _write_data_byte(0x03);
    _write_data_byte(0x11); _write_data_byte(0x07);
    _write_data_byte(0x31); _write_data_byte(0xC1);
    _write_data_byte(0x48); _write_data_byte(0x08);
    _write_data_byte(0x0F); _write_data_byte(0x0C);
    _write_data_byte(0x31); _write_data_byte(0x36);
    _write_data_byte(0x0F);

    _write_cmd(0x11); HAL_Delay(120);   // Sleep Out
    _write_cmd(0x29); HAL_Delay(20);    // Display ON

    _cs_high();
}

/* ── SetWindow: CS LOW 유지한 채 반환 ─────────────────── */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t buf[4];

    _cs_low();

    buf[0]=x0>>8; buf[1]=x0&0xFF; buf[2]=x1>>8; buf[3]=x1&0xFF;
    _write_cmd(0x2A);
    _write_data(buf, 4);

    buf[0]=y0>>8; buf[1]=y0&0xFF; buf[2]=y1>>8; buf[3]=y1&0xFF;
    _write_cmd(0x2B);
    _write_data(buf, 4);

    _write_cmd(0x2C);   // Memory Write — CS는 LOW 유지
}

/* ── DrawRect: 한 줄 버퍼로 묶어서 전송 ───────────────── */
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);  // CS LOW 상태 반환

    static uint8_t line_buf[LCD_WIDTH * 2];       // 480바이트
    uint8_t hi = color >> 8, lo = color & 0xFF;
    for (uint16_t i = 0; i < w; i++) {
        line_buf[i * 2]     = hi;
        line_buf[i * 2 + 1] = lo;
    }

    _dc_data();
    for (uint16_t row = 0; row < h; row++) {
        HAL_SPI_Transmit(&hspi1, line_buf, w * 2, 100);
    }

    _cs_high();
}

void LCD_FillScreen(uint16_t color) {
    LCD_DrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void LCD_DrawHollowRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t thick, uint16_t color) {
    // 윗변 (Top)
    LCD_DrawRect(x, y, w, thick, color);
    // 아랫변 (Bottom)
    LCD_DrawRect(x, y + h - thick, w, thick, color);
    // 왼쪽 변 (Left)
    LCD_DrawRect(x, y, thick, h, color);
    // 오른쪽 변 (Right)
    LCD_DrawRect(x + w - thick, y, thick, h, color);
}

/* ── DrawImage: 영상 데이터 한 번에 전송 ──────────────── */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t *data) {
    LCD_SetWindow(x, y, x + w - 1, y + h - 1);  // CS LOW 상태 반환

    _dc_data();
    HAL_SPI_Transmit(&hspi1, data, (uint32_t)w * h * 2, 2000);

    _cs_high();
}