#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RED_H_MIN_LOW    0
#define RED_H_MAX_LOW   10
#define RED_H_MIN_HIGH 170
#define RED_H_MAX_HIGH 179
#define RED_S_MIN      150
#define RED_S_MAX      255
#define RED_V_MIN       80
#define RED_V_MAX      255

#define MIN_DETECT_AREA  200

// SPI 전송 시 메모리를 유지하기 위해 구조체에 fb 포인터 추가
typedef struct {
    bool detected;
    int  cx;
    int  cy;
    int  area;
    int  frame_w;
    int  frame_h;
    camera_fb_t *fb; // 카메라 이미지 원본 데이터
} TrackResult;

bool        colorTracker_init(void);
TrackResult colorTracker_process(void);
void        colorTracker_free(TrackResult *r); // 전송 후 메모리 해제용

void        rgb565_to_hsv(uint16_t pixel, uint8_t *h, uint8_t *s, uint8_t *v);

#ifdef __cplusplus
}
#endif