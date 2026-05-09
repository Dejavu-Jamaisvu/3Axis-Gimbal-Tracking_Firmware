#include "wifi_manager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>

#define WIFI_SSID "KCCI601"
#define WIFI_PASS "@kcci601@"

WebServer server(80);

// 스트리밍 태스크 관련 변수 (내부에서만 사용하므로 static 처리)
static TaskHandle_t stream_task_handle = NULL;
static WiFiClient stream_client;
static volatile bool stream_active = false;

// 스트리밍 태스크
void stream_task(void *arg) {
    while (1) {
        if (stream_active && stream_client.connected()) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb) {
                uint8_t *jpeg_buf = NULL;
                size_t   jpeg_len = 0;
                bool ok = frame2jpg(fb, 80, &jpeg_buf, &jpeg_len);
                esp_camera_fb_return(fb);

                if (ok) {
                    stream_client.printf("--frame\r\n");
                    stream_client.printf("Content-Type: image/jpeg\r\n");
                    stream_client.printf("Content-Length: %d\r\n\r\n", jpeg_len);
                    stream_client.write(jpeg_buf, jpeg_len);
                    stream_client.printf("\r\n");
                    free(jpeg_buf);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1));  // 블로킹 방지
        } else {
            stream_active = false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

// 스트리밍 핸들러
void handle_stream() {
    stream_client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    stream_client.print(response);
    stream_active = true;  // 태스크에게 시작 신호
}

// 좌표 API
void handle_status() {
    String json = "{";
    json += "\"detected\":"  + String(g_detected ? "true" : "false") + ",";
    json += "\"cx\":"        + String(g_cx)   + ",";
    json += "\"cy\":"        + String(g_cy)   + ",";
    json += "\"area\":"      + String(g_area);
    json += "}";
    server.send(200, "application/json", json);
}

// 메인 페이지
void handle_root() {
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>ESP32-CAM Tracker</title>
    <style>
        * { margin:0; padding:0; box-sizing:border-box; }
        body { background: #111; color: #fff; font-family: Arial, sans-serif; display: flex; flex-direction: column; align-items: center; padding: 20px; }
        h2 { color: #ff4444; margin-bottom: 15px; }
        #container { position: relative; display: inline-block; border: 2px solid #333; }
        #stream { width: 640px; height: 480px; display: block; }
        #overlay { position: absolute; top: 0; left: 0; width: 640px; height: 480px; pointer-events: none; }
        #cross_h { position: absolute; width: 100%; height: 1px; background: rgba(255,255,0,0.25); top: 50%; pointer-events: none; }
        #cross_v { position: absolute; height: 100%; width: 1px; background: rgba(255,255,0,0.25); left: 50%; pointer-events: none; }
        #status_box { margin-top: 15px; background: #1e1e1e; border: 1px solid #333; border-radius: 10px; padding: 15px 30px; width: 640px; }
        .row { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #2a2a2a; }
        .row:last-child { border-bottom: none; }
        .label { color: #aaa; font-size: 14px; }
        .val   { color: #00ff88; font-weight: bold; font-size: 14px; }
        #badge { display: inline-block; padding: 3px 14px; border-radius: 12px; font-weight: bold; font-size: 13px; }
        .on  { background: #ff0000; color: #fff; }
        .off { background: #333;    color: #777; }
    </style>
</head>
<body>
    <h2>ESP32-CAM Red Object Tracker</h2>
    <div id="container">
        <img id="stream" src="/stream">
        <div id="cross_h"></div>
        <div id="cross_v"></div>
        <canvas id="overlay" width="640" height="480"></canvas>
    </div>
    <div id="status_box">
        <div class="row"><span class="label">감지 상태</span><span id="badge" class="off">NO TARGET</span></div>
        <div class="row"><span class="label">X 좌표</span><span class="val" id="v_cx">-</span></div>
        <div class="row"><span class="label">Y 좌표</span><span class="val" id="v_cy">-</span></div>
        <div class="row"><span class="label">면적</span><span class="val" id="v_area">-</span></div>
        <div class="row"><span class="label">거리 추정</span><span class="val" id="v_dist">-</span></div>
    </div>
    <script>
        const SW=640, SH=480, CW=160, CH=120;
        const SX=SW/CW, SY=SH/CH;
        const canvas = document.getElementById('overlay');
        const ctx    = canvas.getContext('2d');
        const badge  = document.getElementById('badge');

        function estimateDist(area) {
            if (area > 3000) return '매우 가까움 (~10cm)';
            if (area > 1500) return '가까움 (~20cm)';
            if (area >  500) return '보통 (~40cm)';
            if (area >  100) return '멀리 있음 (~80cm)';
            return '매우 멀리 있음';
        }

        function drawTracker(cx, cy, area) {
            ctx.clearRect(0, 0, SW, SH);
            let sx = cx * SX; let sy = cy * SY;
            let side = Math.max(Math.sqrt(area) * SX, 30);
            let half = side / 2;
            ctx.strokeStyle = '#ff0000'; ctx.lineWidth = 3; ctx.shadowColor = '#ff0000'; ctx.shadowBlur = 8;
            ctx.strokeRect(sx - half, sy - half, side, side);
            ctx.fillStyle = '#ff0000'; ctx.beginPath(); ctx.arc(sx, sy, 4, 0, Math.PI*2); ctx.fill();
            ctx.shadowBlur = 0; ctx.fillStyle = '#ff0000'; ctx.font = 'bold 13px Arial';
            ctx.fillText('(' + cx + ', ' + cy + ')', sx - half, sy - half - 6);
        }

        setInterval(() => {
            fetch('/status').then(r => r.json()).then(d => {
                document.getElementById('v_cx').innerText   = d.cx;
                document.getElementById('v_cy').innerText   = d.cy;
                document.getElementById('v_area').innerText = d.area;
                document.getElementById('v_dist').innerText = estimateDist(d.area);
                if (d.detected) {
                    drawTracker(d.cx, d.cy, d.area);
                    badge.className = 'on'; badge.innerText = 'TRACKING';
                } else {
                    ctx.clearRect(0, 0, SW, SH);
                    badge.className = 'off'; badge.innerText = 'NO TARGET';
                }
            }).catch(() => {});
        }, 50);
    </script>
</body>
</html>
    )";
    server.send(200, "text/html", html);
}

// WiFi 초기화
void wifi_init() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[WiFi] 연결 중");

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 40) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n================================");
        Serial.print(">>> http://");
        Serial.println(WiFi.localIP());
        Serial.println("================================");
    } else {
        Serial.println("\n[WiFi] 연결 실패");
    }

    server.on("/",       handle_root);
    server.on("/stream", handle_stream);
    server.on("/status", handle_status);
    server.begin();

    xTaskCreatePinnedToCore(
        stream_task,
        "stream_task",
        8192,
        NULL,
        1,
        &stream_task_handle,
        0
    );
}

// 외부에서 호출할 Wrapper 함수들 
void wifi_handle_client() {
    server.handleClient();
}

void print_wifi_ip_periodically() {
    static uint32_t ip_print_time = 0;
    if (millis() - ip_print_time > 10000) {
        ip_print_time = millis();
        if (WiFi.status() == WL_CONNECTED) {
            Serial.print(">>> http://");
            Serial.println(WiFi.localIP());
        }
    }
}