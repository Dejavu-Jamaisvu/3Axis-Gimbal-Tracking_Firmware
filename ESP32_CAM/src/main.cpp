#include <Arduino.h>
#include <SPI.h>
#include "color_tracker.h"

#define DEBUG_BAUD        115200
#define LOOP_INTERVAL_MS  50      /* 20fps */

/* HSPI 핀 설정 (STM32와 연결될 핀들) */
#define HSPI_SCLK 14
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_CS   15

SPIClass *hspi = NULL;

void setup()
{
    Serial.begin(DEBUG_BAUD);
    delay(500);
    Serial.println("=== ESP32-CAM SPI Video & Red Tracker ===");

    // HSPI 초기화
    hspi = new SPIClass(HSPI);
    hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
    pinMode(HSPI_CS, OUTPUT);
    digitalWrite(HSPI_CS, HIGH); // 평상시에는 HIGH (대기 상태)
    Serial.println("[SPI] HSPI 초기화 완료 (14:CLK, 12:MISO, 13:MOSI, 15:CS)");

    if (!colorTracker_init()) {
        Serial.println("[ERROR] 카메라 초기화 실패 → 재시작");
        delay(3000);
        ESP.restart();
    }

    Serial.println("[READY] 실시간 스트리밍 & 빨강 추적 시작");
}

void loop()
{
    static uint32_t last_time = 0;
    if (millis() - last_time < LOOP_INTERVAL_MS) return;
    last_time = millis();

    // 1. 영상 캡처 및 좌표 계산
    TrackResult result = colorTracker_process();

    if (result.fb) {
        // 2. SPI 통신 시작 (10MHz 속도로 설정)
        hspi->beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
        digitalWrite(HSPI_CS, LOW); // STM32에게 "데이터 간다!" 신호 보냄

        // [패킷 1] 헤더 (STM32가 시작점을 찾을 수 있게)
        hspi->transfer(0xAA);
        hspi->transfer(0xBB);

        // [패킷 2] 영상 데이터 통째로 전송 (엄청난 속도!)
        hspi->writeBytes(result.fb->buf, result.fb->len);

        // [패킷 3] 추적된 좌표 데이터 전송
        hspi->transfer((result.cx >> 8) & 0xFF);
        hspi->transfer(result.cx & 0xFF);
        hspi->transfer((result.cy >> 8) & 0xFF);
        hspi->transfer(result.cy & 0xFF);
        hspi->transfer(result.detected ? 0x01 : 0x00);

        // [패킷 4] 푸터 (전송 끝)
        hspi->transfer(0xCC);
        hspi->transfer(0xDD);

        digitalWrite(HSPI_CS, HIGH); // 전송 종료
        hspi->endTransaction();

        // 3. 사용이 끝난 프레임 메모리 반환 (매우 중요!)
        colorTracker_free(&result);
    }

    // 디버그용 출력
    if (result.detected) {
        Serial.printf("[TRACK] CX=%d  CY=%d  AREA=%d\n", result.cx, result.cy, result.area);
    } else {
        Serial.println("[TRACK] 타겟 없음");
    }
}