#include "color_tracker.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "CAM";

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

bool colorTracker_init(void)
{
    camera_config_t config;
    memset(&config, 0, sizeof(config));

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size   = FRAMESIZE_QVGA;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "카메라 초기화 실패: 0x%x", err);
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_saturation(s,    2);
    s->set_contrast(s,      1);
    s->set_brightness(s,    0);
    s->set_whitebal(s,      1);
    s->set_exposure_ctrl(s, 1);

    ESP_LOGI(TAG, "카메라 초기화 성공 (QVGA RGB565)");
    return true;
}

void rgb565_to_hsv(uint16_t pixel, uint8_t *h, uint8_t *s, uint8_t *v)
{
    uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((pixel >>  5) & 0x3F) * 255 / 63;
    uint8_t b = ( pixel        & 0x1F) * 255 / 31;

    uint8_t cmax = r > g ? (r > b ? r : b) : (g > b ? g : b);
    uint8_t cmin = r < g ? (r < b ? r : b) : (g < b ? g : b);
    uint8_t diff = cmax - cmin;

    *v = cmax;
    *s = (cmax == 0) ? 0 : (uint8_t)((uint16_t)diff * 255 / cmax);

    if (diff == 0) {
        *h = 0;
    } else if (cmax == r) {
        int16_t tmp = (int16_t)(30 * (int16_t)(g - b) / diff);
        if (g < b) tmp += 180;
        *h = (uint8_t)tmp;
    } else if (cmax == g) {
        *h = (uint8_t)(30 * (int16_t)(b - r) / diff + 60);
    } else {
        *h = (uint8_t)(30 * (int16_t)(r - g) / diff + 120);
    }
}

static bool is_red_pixel(uint8_t h, uint8_t s, uint8_t v)
{
    if (s < RED_S_MIN || s > RED_S_MAX) return false;
    if (v < RED_V_MIN || v > RED_V_MAX) return false;

    bool hue_low  = (h >= RED_H_MIN_LOW  && h <= RED_H_MAX_LOW);
    bool hue_high = (h >= RED_H_MIN_HIGH && h <= RED_H_MAX_HIGH);

    return hue_low || hue_high;
}

TrackResult colorTracker_process(void)
{
    TrackResult result;
    memset(&result, 0, sizeof(result));
    result.frame_w = 320;
    result.frame_h = 240;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "프레임 캡처 실패");
        return result;
    }

    result.frame_w = (int)fb->width;
    result.frame_h = (int)fb->height;

    int       total  = (int)(fb->width * fb->height);
    uint16_t *pixels = (uint16_t *)fb->buf;

    long sum_x = 0, sum_y = 0;
    int  count  = 0;

    for (int i = 0; i < total; i++) {
        uint16_t raw = pixels[i];
        uint16_t px  = (uint16_t)((raw >> 8) | (raw << 8));

        uint8_t h, s, v;
        rgb565_to_hsv(px, &h, &s, &v);

        if (is_red_pixel(h, s, v)) {
            sum_x += i % (int)fb->width;
            sum_y += i / (int)fb->width;
            count++;
        }
    }

    esp_camera_fb_return(fb);

    if (count >= MIN_DETECT_AREA) {
        result.detected = true;
        result.cx       = (int)(sum_x / count);
        result.cy       = (int)(sum_y / count);
        result.area     = count;
    }

    return result;
}