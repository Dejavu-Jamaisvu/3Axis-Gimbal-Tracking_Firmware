#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

// 빨간색 HSV 범위
#define RED_H_MIN_LOW    0
#define RED_H_MAX_LOW   10      // (기존 15 -> 10) 주황색 빛이 도는 피부색 차단
#define RED_H_MIN_HIGH 170      // (기존 165 -> 170) 자주색(마젠타) 계열 약간 축소
#define RED_H_MAX_HIGH 179
#define RED_S_MIN       150     // (기존 80 -> 150) 연한 살구색/핑크색 차단
#define RED_S_MAX      255
#define RED_V_MIN       60      // (기존 50 -> 60) 너무 어두운 그림자 노이즈 약간 컷
#define RED_V_MAX      255

#define MIN_DETECT_AREA  50 // 최소 50픽셀 이상일때부터 인식

// 칼만 필터 변수
typedef struct {
    float x, p, q, r, k;
} KalmanFilter;

// 발견된 덩어리(Blob) 정보
typedef struct {
    int cx, cy;
    int area;
    int red_score;   // 얼마나 순수한 빨강인가
    int bbox_x, bbox_y, bbox_w, bbox_h;
} Blob;

// 최종 추적 결과
typedef struct {
    bool  detected;
    int   cx, cy;
    int   area;
    int   red_score;
    int   frame_w, frame_h;
    int   bbox_x, bbox_y, bbox_w, bbox_h;
    camera_fb_t *fb; // 프레임 버퍼 포인터
} TrackResult;

bool        colorTracker_init(void);
TrackResult colorTracker_process(void);
void        colorTracker_free(TrackResult *r);
void        rgb565_to_hsv(uint16_t pixel, uint8_t *h, uint8_t *s, uint8_t *v);

#ifdef __cplusplus
}
#endif