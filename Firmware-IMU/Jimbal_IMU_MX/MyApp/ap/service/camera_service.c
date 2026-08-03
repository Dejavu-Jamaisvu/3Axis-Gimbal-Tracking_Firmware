#include "camera_service.h"

// 최신 카메라 데이터를 저장할 구조체
static camera_data_t cam_data;

// [수정: PID 결과를 저장할 내부 변수]
static float filtered_pid_x = 0.0f;
static float filtered_pid_y = 0.0f;

// 카메라 서비스 내부에서 직접 누적할 절대 오프셋 변수
static float absolute_cam_offset_x = 0.0f;
static float absolute_cam_offset_y = 0.0f;

// [PID 상태 변수] 이전 오차와 누적 오차를 기억하기 위함
static float error_x_sum = 0.0f, error_x_prev = 0.0f;
static float error_y_sum = 0.0f, error_y_prev = 0.0f;

void cameraServiceInit(void) {
    // 초기화 시 모든 데이터를 0으로 세팅
    cam_data.x = 80;
    cam_data.y = 60;
    cam_data.area = 0;
    cam_data.is_detected = false;

    error_x_sum = error_x_prev = 0.0f;
    error_y_sum = error_y_prev = 0.0f;

    absolute_cam_offset_x = 0.0f;
    absolute_cam_offset_y = 0.0f;
}

camera_data_t* cameraGetLatestData(void) {
    return &cam_data;
}



bool cameraDataParsing(void) {
    uint8_t data;
    static uint8_t buf[8]; // 9바이트 패킷 버퍼[cite: 5]
    static uint8_t idx = 0;
    
    bool new_data_received = false;

    // UART로부터 데이터 수신 (Board A -> Board B)
    while(uartAvailable(1) > 0) {
        data = uartRead(1);

        // STX(0x02) 찾기
        if (idx == 0) {
            if (data == 0x02) buf[idx++] = data;
            continue;
        }

        // 데이터 채우기
        if (idx < 7) buf[idx++] = data;

        // 패킷 완성 (9바이트)
        if (idx >= 7) {
            if (buf[6] == 0x03) { // ETX 확인
                // 좌표 및 면적 파싱
                cam_data.x = (int16_t)((buf[1] << 8) | buf[2]);
                cam_data.y = (int16_t)((buf[3] << 8) | buf[4]);
                // cam_data.area = (uint16_t)((buf[5] << 8) | buf[6]);
                cam_data.is_detected = (buf[5] == 0x01);
                
                // [테스트 로그] STM32 터미널에서 확인용
                if (cam_data.is_detected) {
                    // cliPrintf("[STM32_LOG] CX=%d CY=%d AREA=%d\r\n", cam_data.x, cam_data.y, cam_data.area);
                    
                    cliPrintf("[STM32_LOG] CX=%d CY=%d \r\n", cam_data.x, cam_data.y);
                } else {
                    cliPrintf("[STM32_LOG] 타겟 없음\r\n");
                }
                // 파싱 완료 즉시 짐벌 제어 수행
                // cameraServicePIDUpdate(); 


                // 패킷이 완벽하게 파싱되었을 때만 true로 바꿈!
                new_data_received = true;
                
            }
            idx = 0; // 다음 패킷 준비
        }
    }
    // 루프를 다 돌고 나서 결과를 반환
    return new_data_received;
}


// void cameraServicePIDUpdate(void) {
//     if (!cam_data.is_detected) {
//         // 물체를 놓치면 급격한 회전을 방지하기 위해 누적 데이터 초기화[cite: 5]
//         error_x_sum = error_x_prev = error_y_sum = error_y_prev = 0.0f;
//         return;
//     }

//     // 기본 게인 설정 
//     float Kp = 0.15f;
//     float Ki = 0.005f; // 잔류 오차 제거용[cite: 5]
//     float Kd = 0.02f;  // 진동 억제(브레이크)용

//     // 면적이 크면(가까우면) 부드럽게, 작으면(멀면) 민감하게 반응
//     if (cam_data.area > 5000) {      
//         Kp = 0.10f; Kd = 0.04f; // 가까울 때: 감도 낮추고 브레이크 강화
//     } else if (cam_data.area < 500) { 
//         Kp = 0.22f; Ki = 0.008f; // 멀 때: 감도 높이고 끈기 있게 추적
//     }

//     float err_x = (float)(80 - cam_data.x);
//     float err_y = (float)(cam_data.y - 60);

//     error_x_sum += err_x;
//     if (error_x_sum > 20.0f) error_x_sum = 20.0f;   // Anti-windup (한계 설정)
//     if (error_x_sum < -20.0f) error_x_sum = -20.0f;
//     float d_err_x = err_x - error_x_prev;
//     float pid_x = (err_x * Kp) + (error_x_sum * Ki) + (d_err_x * Kd);
//     error_x_prev = err_x;

//     error_y_sum += err_y;
//     if (error_y_sum > 20.0f) error_y_sum = 20.0f;
//     if (error_y_sum < -20.0f) error_y_sum = -20.0f;
//     float d_err_y = err_y - error_y_prev;
//     float pid_y = (err_y * Kp) + (error_y_sum * Ki) + (d_err_y * Kd);
//     error_y_prev = err_y;

//     gimbalSettingCamOffset(pid_x, pid_y);
// }



void cameraServicePIDUpdate(void) {
    static uint32_t last_print_tick = 0; // 로그 출력 타이밍 관리
    uint32_t now = osKernelGetTickCount();


    // 1. 타겟 상실 시 처리
    if (!cam_data.is_detected) {
        error_x_sum = error_x_prev = 0.0f;
        error_y_sum = error_y_prev = 0.0f;
        
        // 🌟 타겟을 놓치면 누적된 오프셋을 서서히 0(정면)으로 복귀시킵니다.
        // 0.95f 숫자가 클수록 천천히, 작을수록 빠르게 복귀합니다.
        // absolute_cam_offset_x *= 0.95f; 
        // absolute_cam_offset_y *= 0.95f;


        filtered_pid_x *= 0; 
        filtered_pid_y *= 0;
        // gimbalSettingCamOffset(filtered_pid_x, filtered_pid_y);
        return;
    }

    // 2. 기본 게인 및 오차 계산
    // float Kp = 0.15f;
    // float Ki = 0.005f;
    // float Kd = 0.02f;
    float Kp = 0.4f;  // 기존 0.25에서 대폭 상향
    float Ki = 0.0f; // 누적 오차를 더 빨리 반영 (0.015 -> 0.03)
    float Kd = 0.06f;  // 움직일 때 제동을 걸어 오버슈트 방지

    if (cam_data.area > 5000) {      
        Kp = 0.10f; Kd = 0.04f;
    } else if (cam_data.area < 500) { 
        Kp = 0.22f; Ki = 0.008f;
    }

    float err_x = (float)(80 - cam_data.x);
    float err_y = (float)(60 - cam_data.y);

    // ---------------------------------------------------------
    // 🎯 [핵심 추가] 3. 데드존 (Deadzone) 설정
    // 오차가 ±3 픽셀 이내면 중앙으로 간주하여 진동 억제
    // ---------------------------------------------------------
    // if (fabsf(err_x) < 1.0f) {
    //     err_x = 0.0f;
    //     error_x_sum = 0.0f; // I항 누적으로 인한 흔들림 방지
    // }
    // if (fabsf(err_y) < 1.0f) {
    //     err_y = 0.0f;
    //     error_y_sum = 0.0f;
    // }

    // 4. PID 계산 (X축)
    error_x_sum += err_x;
    if (error_x_sum > 100.0f) error_x_sum = 100.0f;
    if (error_x_sum < -100.0f) error_x_sum = -100.0f;
    float d_err_x = err_x - error_x_prev;
    float target_pid_x = (err_x * Kp) + (error_x_sum * Ki) + (d_err_x * Kd);
    error_x_prev = err_x;

    // 5. PID 계산 (Y축)
    error_y_sum += err_y;
    if (error_y_sum > 100.0f) error_y_sum = 100.0f;
    if (error_y_sum < -100.0f) error_y_sum = -100.0f;
    float d_err_y = err_y - error_y_prev;
    float target_pid_y = (err_y * Kp) + (error_y_sum * Ki) + (d_err_y * Kd);
    error_y_prev = err_y;

    // ---------------------------------------------------------
    // 🌊 [핵심 추가] 6. 저역 통과 필터 (LPF) 적용
    // 계산된 PID 값이 급격하게 튀는 것을 방지 (0.1이 반응성, 숫자가 작을수록 부드러움)
    // ---------------------------------------------------------
    filtered_pid_x = (filtered_pid_x * 0.4f) + (target_pid_x * 0.6f); //target_pid_x; // (filtered_pid_x * 0.2f) + (target_pid_x * 0.8f);
    filtered_pid_y = (filtered_pid_y * 0.4f) + (target_pid_y * 0.6f);//target_pid_y; // (filtered_pid_y * 0.2f) + (target_pid_y * 0.8f);


    // 🌟 [핵심] 카메라 주기(30Hz)에 맞춰 1번만 누적됩니다.
    absolute_cam_offset_x += (filtered_pid_x * -0.1f);
    absolute_cam_offset_y += (filtered_pid_y * 0.1f);


    // ---------------------------------------------------------
    // 🛠 [디버깅 로그 추가] 0.1초마다 현재 상태 출력
    // ---------------------------------------------------------
    if (now - last_print_tick > 100) {
        cliPrintf("[CAM] Pos(%3d,%3d) | Err(%3.1f,%3.1f) | PID_Out(%3.2f,%3.2f)\r\n", 
                  cam_data.x, cam_data.y, err_x, err_y, filtered_pid_x, filtered_pid_y);
        last_print_tick = now;
    }
}

// [수정: Pull 방식용 Getter 함수 구현]
// void cameraServiceGetPIDOffset(float *out_x, float *out_y) {
//     if (out_x) *out_x = filtered_pid_x;
//     if (out_y) *out_y = filtered_pid_y;
// }
void cameraServiceGetPIDOffset(float *out_x, float *out_y) {
    if (out_x) *out_x = absolute_cam_offset_x;
    if (out_y) *out_y = absolute_cam_offset_y;
}


void cameraServiceResetZeroPoint(void) {
    absolute_cam_offset_x = 0.0f;
    absolute_cam_offset_y = 0.0f;

    // 2. 🌟 PID 중간 변수 및 필터 잔상 제거 (치우침 방지의 핵심)
    filtered_pid_x = 0.0f;
    filtered_pid_y = 0.0f;
    error_x_sum = 0.0f;
    error_y_sum = 0.0f;
    error_x_prev = 0.0f;
    error_y_prev = 0.0f;

    cliPrintf("[CAM] Camera Offset Reset to Zero.\r\n");
}