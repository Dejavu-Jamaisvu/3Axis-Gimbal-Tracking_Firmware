#include <Arduino.h>
#include <SPI.h>
#include <esp_camera.h>
#include "color_tracker.h"
#include "wifi_manager.h"

#define DEBUG_BAUD        115200
#define LOOP_INTERVAL_MS  50

#define HSPI_SCLK 14
#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_CS   15

SPIClass *hspi = NULL;

// 공유 전역 변수 (실제 메모리 할당)
volatile int  g_cx = 0, g_cy = 0, g_area = 0;
volatile bool g_detected = false;

// ── setup ──────────────────────────────────────────
void setup() {
    Serial.begin(DEBUG_BAUD);
    delay(500);

    // SPI 초기화
    hspi = new SPIClass(HSPI);
    hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
    pinMode(HSPI_CS, OUTPUT);
    digitalWrite(HSPI_CS, HIGH);

    // 카메라 초기화
    if (!colorTracker_init()) {
        Serial.println("[ERROR] 카메라 초기화 실패");
        delay(3000);
        ESP.restart();
    }
    wifi_init();
    
    Serial.println("[READY] 시작!");
}

// ── loop (코어 1에서 실행) 
void loop() {
    // 웹서버 접속 처리 및 주기적 IP 출력 (wifi_manager 모듈 함수 호출)
    wifi_handle_client();
    print_wifi_ip_periodically();

    static uint32_t last_time = 0;
    if (millis() - last_time < LOOP_INTERVAL_MS) return;
    last_time = millis();

    // 추적 수행
    TrackResult result = colorTracker_process();

    // 웹서버용 공유 변수 업데이트
    g_cx       = result.cx;
    g_cy       = result.cy;
    g_area     = result.area;
    g_detected = result.detected;

    // SPI 통신 수행
    if (result.fb) {
        hspi->beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
        digitalWrite(HSPI_CS, LOW);

        hspi->transfer(0xAA);
        hspi->transfer(0xBB);
        hspi->writeBytes(result.fb->buf, result.fb->len);
        hspi->transfer((result.cx >> 8) & 0xFF);
        hspi->transfer( result.cx       & 0xFF);
        hspi->transfer((result.cy >> 8) & 0xFF);
        hspi->transfer( result.cy       & 0xFF);
        hspi->transfer(result.detected ? 0x01 : 0x00);
        hspi->transfer(0xCC);
        hspi->transfer(0xDD);

        digitalWrite(HSPI_CS, HIGH);
        hspi->endTransaction();

        colorTracker_free(&result);
    }

    // 터미널 디버그 출력
    if (result.detected) {
        Serial.printf("[TRACK] CX=%d CY=%d AREA=%d\n", result.cx, result.cy, result.area);
    } else {
        Serial.println("[TRACK] 타겟 없음");
    }
}

// #include <Arduino.h>
// #include <SPI.h>
// #include "color_tracker.h"

// #define DEBUG_BAUD        115200
// #define LOOP_INTERVAL_MS  50      /* 20fps */

// /* HSPI 핀 설정 (STM32와 연결될 핀들) */
// #define HSPI_SCLK 14
// #define HSPI_MISO 12
// #define HSPI_MOSI 13
// #define HSPI_CS   15

// SPIClass *hspi = NULL;

// void setup()
// {
//     Serial.begin(DEBUG_BAUD);
//     delay(500);
//     Serial.println("=== ESP32-CAM SPI Video & Red Tracker ===");

//     // HSPI 초기화
//     hspi = new SPIClass(HSPI);
//     hspi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);
//     pinMode(HSPI_CS, OUTPUT);
//     digitalWrite(HSPI_CS, HIGH); // 평상시에는 HIGH (대기 상태)
//     Serial.println("[SPI] HSPI 초기화 완료 (14:CLK, 12:MISO, 13:MOSI, 15:CS)");

//     if (!colorTracker_init()) {
//         Serial.println("[ERROR] 카메라 초기화 실패 → 재시작");
//         delay(3000);
//         ESP.restart();
//     }

//     Serial.println("[READY] 실시간 스트리밍 & 빨강 추적 시작");
// }

// void loop()
// {
//     static uint32_t last_time = 0;
//     if (millis() - last_time < LOOP_INTERVAL_MS) return;
//     last_time = millis();

//     // 영상 캡처 및 좌표 계산
//     TrackResult result = colorTracker_process();

//     if (result.fb) {
//         // SPI 통신 시작 (10MHz 속도로 설정)
//         hspi->beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
//         digitalWrite(HSPI_CS, LOW); // STM32에게 "데이터 간다!" 신호 보냄

//         // [패킷 1] 헤더 (STM32가 시작점을 찾을 수 있게)
//         hspi->transfer(0xAA);
//         hspi->transfer(0xBB);

//         // [패킷 2] 영상 데이터 통째로 전송
//         hspi->writeBytes(result.fb->buf, result.fb->len);

//         // [패킷 3] 추적된 좌표 데이터 전송
//         hspi->transfer((result.cx >> 8) & 0xFF);
//         hspi->transfer(result.cx & 0xFF);
//         hspi->transfer((result.cy >> 8) & 0xFF);
//         hspi->transfer(result.cy & 0xFF);
//         hspi->transfer(result.detected ? 0x01 : 0x00);

//         // [패킷 4] 푸터
//         hspi->transfer(0xCC);
//         hspi->transfer(0xDD);

//         digitalWrite(HSPI_CS, HIGH); // 전송 종료
//         hspi->endTransaction();

//         // 사용이 끝난 프레임 메모리 반환
//         colorTracker_free(&result);
//     }

//     // 디버그용 출력
//     if (result.detected) {
//         Serial.printf("[TRACK] CX=%d  CY=%d  AREA=%d\n", result.cx, result.cy, result.area);
//     } else {
//         Serial.println("[TRACK] 타겟 없음");
//     }
// }