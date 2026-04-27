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

#define FRAME_STX  0x02
#define FRAME_ETX  0x03

typedef struct {
    bool detected;
    int  cx;
    int  cy;
    int  area;
    int  frame_w;
    int  frame_h;
} TrackResult;

bool        colorTracker_init(void);
TrackResult colorTracker_process(void);
void        rgb565_to_hsv(uint16_t pixel, uint8_t *h, uint8_t *s, uint8_t *v);

#ifdef __cplusplus
}
#endif