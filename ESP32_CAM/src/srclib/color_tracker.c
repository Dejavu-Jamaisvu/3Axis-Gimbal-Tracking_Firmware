#include "color_tracker.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "CAM";

// ESP32-CAM 핀 맵 
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define MAX_BLOBS 5
#define AREA_SIMILAR_RATIO 5  // 면적 차이 20% 이내는 비슷한 것으로 간주

// 칼만 필터 초기화 (X, Y 축 각각 적용)
static KalmanFilter kf_x = {0, 1, 0.1f, 5.0f, 0};
static KalmanFilter kf_y = {0, 1, 0.1f, 5.0f, 0};
static int prev_cx = -1, prev_cy = -1;

/* ── 칼만 필터 업데이트 (좌표 떨림 방지) ── */
static float kalman_update(KalmanFilter *kf, float measurement) {
    kf->p = kf->p + kf->q;
    kf->k = kf->p / (kf->p + kf->r);
    kf->x = kf->x + kf->k * (measurement - kf->x);
    kf->p = (1 - kf->k) * kf->p;
    return kf->x;
}

/* ── 카메라 하드웨어 초기화 ── */
bool colorTracker_init(void) {
    camera_config_t config;
    memset(&config, 0, sizeof(config));

    // 핀 및 해상도 설정 (QQVGA, RGB565)
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM; 
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM; 
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM; 
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM; 
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM; 
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM; 
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; 
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM; 
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_QQVGA;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    if (esp_camera_init(&config) != ESP_OK) return false;

    // 카메라 색감 보정
    sensor_t *s = esp_camera_sensor_get();
    s->set_saturation(s, 2);
    s->set_contrast(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_awb_gain(s, 1);

    return true;
}

/* ── 빛 반사에 강한 HSV 색상계로 변환 ── */
void rgb565_to_hsv(uint16_t pixel, uint8_t *h, uint8_t *s, uint8_t *v) {
    uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((pixel >>  5) & 0x3F) * 255 / 63;
    uint8_t b = ( pixel        & 0x1F) * 255 / 31;

    uint8_t cmax = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint8_t cmin = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint8_t diff = cmax - cmin;

    *v = cmax;
    *s = (cmax == 0) ? 0 : (uint8_t)((uint32_t)diff * 255 / cmax);
    if (diff == 0) { *h = 0; return; }

    float hf;
    if      (cmax == r) { hf = 30.0f * (float)(g - b) / diff; if (g < b) hf += 180.0f; }
    else if (cmax == g) { hf = 30.0f * (float)(b - r) / diff + 60.0f; }
    else                { hf = 30.0f * (float)(r - g) / diff + 120.0f; }
    if (hf < 0) hf += 180.0f;
    *h = (uint8_t)hf;
}

/* ── 정의된 범위 안의 빨간색인지 확인 ── */
static bool is_red_pixel(uint8_t h, uint8_t s, uint8_t v) {
    if (s < RED_S_MIN || v < RED_V_MIN) return false;
    return (h <= RED_H_MAX_LOW) || (h >= RED_H_MIN_HIGH);
}

/* ── 색상과 채도를 바탕으로 빨강 점수(0~100) 부여 ── */
static int calc_red_score(uint8_t h, uint8_t s) {
    int hue_score;
    if (h <= RED_H_MAX_LOW) hue_score = 100 - (h * 100 / RED_H_MAX_LOW);
    else                    hue_score = 100 - ((179 - h) * 100 / (179 - RED_H_MIN_HIGH));
    if (hue_score < 0) hue_score = 0;

    int sat_score = s * 100 / 255;
    return (hue_score * 6 + sat_score * 4) / 10; // 색상 60%, 채도 40% 반영
}

/* ── 여러 타겟 중 가장 추적하기 좋은 타겟 1개 선택 ── */
static int select_best_blob(Blob *blobs, int count) {
    if (count == 0) return -1;
    int best = 0;

    for (int i = 1; i < count; i++) {
        int area_diff = blobs[i].area - blobs[best].area;

        // 1순위: 큰 면적
        if (area_diff > blobs[best].area / AREA_SIMILAR_RATIO) { best = i; continue; }
        if (area_diff < -(blobs[best].area / AREA_SIMILAR_RATIO)) continue;

        // 2순위: 면적이 비슷하면 더 진한 빨간색
        if (blobs[i].red_score > blobs[best].red_score + 10) { best = i; continue; }
        if (blobs[best].red_score > blobs[i].red_score + 10) continue;

        // 3순위: 이전 프레임 위치와 가장 가까운 것
        if (prev_cx >= 0 && prev_cy >= 0) {
            int dist_i    = abs(blobs[i].cx - prev_cx) + abs(blobs[i].cy - prev_cy);
            int dist_best = abs(blobs[best].cx - prev_cx) + abs(blobs[best].cy - prev_cy);
            if (dist_i < dist_best) best = i;
        }
    }
    return best;
}

/* ── 영상 분석 및 타겟 좌표 추출 ── */
TrackResult colorTracker_process(void) {
    TrackResult result;
    memset(&result, 0, sizeof(result));

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return result;

    result.fb = fb;
    result.frame_w = (int)fb->width;
    result.frame_h = (int)fb->height;

    uint16_t *pixels = (uint16_t *)fb->buf;
    Blob blobs[MAX_BLOBS];
    int blob_count = 0;
    memset(blobs, 0, sizeof(blobs));
    long score_sum[MAX_BLOBS] = {0};

    // 이미지 한 줄씩 스캔하며 빨간 픽셀 덩어리(Blob) 찾기
    for (int y = 0; y < fb->height; y++) {
        int run = 0, run_start = 0;
        long run_score = 0;

        for (int x = 0; x <= fb->width; x++) {
            bool red = false;
            int score = 0;

            if (x < fb->width) {
                uint16_t raw = pixels[y * fb->width + x];
                uint16_t px  = (raw >> 8) | (raw << 8); // 엔디안 변환
                uint8_t h, s, v;
                rgb565_to_hsv(px, &h, &s, &v);

                if (is_red_pixel(h, s, v)) {
                    red = true;
                    score = calc_red_score(h, s);
                }
            }

            if (red) {
                if (run == 0) run_start = x;
                run++;
                run_score += score;
            } else {
                if (run >= 2) {
                    int run_cx = run_start + run / 2;
                    bool merged = false;

                    // 기존 덩어리와 x좌표가 비슷하면 병합
                    for (int b = 0; b < blob_count; b++) {
                        if (abs(run_cx - blobs[b].cx) < 30) {
                            int total = blobs[b].area + run;
                            blobs[b].cx = (blobs[b].cx * blobs[b].area + run_cx * run) / total;
                            blobs[b].cy = (blobs[b].cy * blobs[b].area + y * run) / total;
                            blobs[b].area = total;
                            score_sum[b] += run_score;
                            
                            // Bounding Box 확장
                            if (run_start < blobs[b].bbox_x) blobs[b].bbox_x = run_start;
                            if (run_start + run > blobs[b].bbox_x + blobs[b].bbox_w)
                                blobs[b].bbox_w = (run_start + run) - blobs[b].bbox_x;
                            if (y > blobs[b].bbox_y + blobs[b].bbox_h)
                                blobs[b].bbox_h = y - blobs[b].bbox_y;

                            merged = true; break;
                        }
                    }
                    // 새로운 덩어리 생성
                    if (!merged && blob_count < MAX_BLOBS) {
                        blobs[blob_count].cx = run_cx; blobs[blob_count].cy = y;
                        blobs[blob_count].area = run;
                        blobs[blob_count].bbox_x = run_start; blobs[blob_count].bbox_y = y;
                        blobs[blob_count].bbox_w = run; blobs[blob_count].bbox_h = 1;
                        score_sum[blob_count] = run_score;
                        blob_count++;
                    }
                }
                run = 0; run_score = 0;
            }
        }
    }

    // 최소 면적을 넘는 유효한 타겟만 필터링
    Blob valid_blobs[MAX_BLOBS];
    int valid_count = 0;
    for (int b = 0; b < blob_count; b++) {
        if (blobs[b].area >= MIN_DETECT_AREA) {
            blobs[b].red_score = (int)(score_sum[b] / blobs[b].area);
            valid_blobs[valid_count++] = blobs[b];
        }
    }

    // 1등 타겟 선정 및 결과 저장
    int best = select_best_blob(valid_blobs, valid_count);
    if (best >= 0) {
        int box_center_x = valid_blobs[best].bbox_x + (valid_blobs[best].bbox_w / 2);
        int box_center_y = valid_blobs[best].bbox_y + (valid_blobs[best].bbox_h / 2);
        
        result.cx = (int)kalman_update(&kf_x, (float)valid_blobs[best].cx);
        result.cy = (int)kalman_update(&kf_y, (float)valid_blobs[best].cy);
        result.detected = true;
        result.area = valid_blobs[best].area;
        result.red_score = valid_blobs[best].red_score;
        prev_cx = result.cx; prev_cy = result.cy;
    } else {
        prev_cx = -1; prev_cy = -1;
    }

    return result;
}

void colorTracker_free(TrackResult *r) {
    if (r->fb) { esp_camera_fb_return(r->fb); r->fb = NULL; }
}