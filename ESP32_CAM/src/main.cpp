#include <Arduino.h>
#include "color_tracker.h"

#define DEBUG_BAUD        115200
#define STM32_BAUD        115200
#define LOOP_INTERVAL_MS  50      /* 20fps */

HardwareSerial stm32Serial(2);    /* UART2: TX=GPIO12, RX=GPIO13 */

/* ── UART 프레임 송신 (Arduino Serial) ── */
static void sendUART(HardwareSerial &serial, const TrackResult &result)
{
    uint8_t cx_h = (result.cx >> 8) & 0xFF;
    uint8_t cx_l =  result.cx       & 0xFF;
    uint8_t cy_h = (result.cy >> 8) & 0xFF;
    uint8_t cy_l =  result.cy       & 0xFF;
    uint8_t det  =  result.detected ? 0x01 : 0x00;
    uint8_t chk  =  cx_h ^ cx_l ^ cy_h ^ cy_l ^ det;

    uint8_t frame[8] = {
        FRAME_STX,
        cx_h, cx_l,
        cy_h, cy_l,
        det, chk,
        FRAME_ETX
    };

    serial.write(frame, sizeof(frame));
}

void setup()
{
    Serial.begin(DEBUG_BAUD);
    delay(500);
    Serial.println("=== ESP32-CAM Red Tracker ===");

    stm32Serial.begin(STM32_BAUD, SERIAL_8N1, 13, 12);
    Serial.println("[UART2] STM32 시리얼 준비");

    if (!colorTracker_init()) {
        Serial.println("[ERROR] 카메라 초기화 실패 → 재시작");
        delay(3000);
        ESP.restart();
    }

    Serial.println("[READY] 빨강 추적 시작");
}

void loop()
{
    static uint32_t last_time = 0;
    if (millis() - last_time < LOOP_INTERVAL_MS) return;
    last_time = millis();

    TrackResult result = colorTracker_process();

    sendUART(stm32Serial, result);

    if (result.detected) {
        Serial.printf("[TRACK] CX=%d  CY=%d  AREA=%d\n",
                      result.cx, result.cy, result.area);
    } else {
        Serial.println("[TRACK] 타겟 없음");
    }
}