#include "gimbal_control.h"

/* --- 내부 정적 변수 (데이터 저장소) --- */
// 자이로 서비스로부터 받는 기체 각도
// static float imu_roll = 0.0f;
// static float imu_pitch = 0.0f;
// static float imu_yaw = 0.0f;

// 카메라 서비스로부터 받는 추적 보정치 (PID 결과값)
// static float cam_offset_x = 0.0f;
// static float cam_offset_y = 0.0f;

// [수정] 단순 대입용이 아닌, 계속 더해나갈 누적 변수로 변경합니다.
// static float accum_cam_offset_x = 0.0f;
// static float accum_cam_offset_y = 0.0f;

// [추가] 서보 각도 계산을 위한 기준점 변수
// static float yaw_origin = -1.0f;
// static bool is_first_run = true;

/* gimbal_control.c 상단 */
static bool is_cam_control_enable = false; // 기본값 ON

/* 함수 구현 */
void gimbalSetCamControlEnable(bool enable) {
    is_cam_control_enable = enable;
}

bool gimbalGetCamControlEnable(void) {
    return is_cam_control_enable;
}

// /**
//  * @brief 자이로 서비스(gyro_service.c)가 호출하는 데이터 갱신 함수
//  */
// void gimbalSettingIMU(float roll, float pitch, float yaw) {
//     imu_roll = roll *-1 ;
//     imu_pitch = pitch *-1;
//     imu_yaw = 0; //yaw;
// }

// /**
//  * @brief 카메라 서비스(camera_service.c)가 호출하는 데이터 갱신 함수
//  */
// void gimbalSettingCamOffset(float x, float y) {
//     cam_offset_x = x *-1;
//     cam_offset_y = y;
// }

// /**
//  * @brief [중요 수정] 카메라 PID 출력값을 기존 값에 '누적'합니다.
//  */
// void gimbalSettingCamOffset(float x, float y) {
    
//     // x, y는 PID 제어기에서 나온 '보정량'입니다.
//     // 기존 누적값에 이 보정량을 더해줌으로써, 타겟이 중앙에 오면 
//     // 보정량이 0이 되어 현재의 각도를 그대로 유지하게 됩니다.
    
//     // 0.1f는 감도 조절용 가중치입니다. 너무 느리면 키우고, 떨리면 줄이세요.
//     accum_cam_offset_x += (x * -0.1f); 
//     accum_cam_offset_y += (y * 0.1f);

//     // [안전장치] 누적값이 서보의 가동 범위를 벗어나지 않도록 제한합니다.
//     // if (accum_cam_offset_x > 60.0f)  accum_cam_offset_x = 60.0f;
//     // if (accum_cam_offset_x < -60.0f) accum_cam_offset_x = -60.0f;
//     // if (accum_cam_offset_y > 40.0f)  accum_cam_offset_y = 40.0f;
//     // if (accum_cam_offset_y < -40.0f) accum_cam_offset_y = -40.0f;

//     if (accum_cam_offset_x > 110.0f)  accum_cam_offset_x = 110.0f;
//     if (accum_cam_offset_x < -110.0f) accum_cam_offset_x = -110.0f;
//     if (accum_cam_offset_y > 110.0f)  accum_cam_offset_y = 110.0f;
//     if (accum_cam_offset_y < -110.0f) accum_cam_offset_y = -110.0f;
// }


/**
 * @brief [통합 제어부] ap.c에서 주기적으로 호출하여 서보에 최종 명령을 내림
 */
 void gimbalExecuteCombinedControl(void) {

    float imu_r, imu_p, imu_y;
    gyroServiceGetLatestAngles(&imu_r, &imu_p, &imu_y);

    // float cam_pid_x = 0.0f, cam_pid_y = 0.0f;
    // cameraServiceGetPIDOffset(&cam_pid_x, &cam_pid_y);

    // 🌟 1. 누적된 최종 절대 각도만 당겨옴 (Pull)
    float cam_absolute_x = 0.0f, cam_absolute_y = 0.0f;
    cameraServiceGetPIDOffset(&cam_absolute_x, &cam_absolute_y);
    
    // if (is_cam_control_enable) {
    //     // 카메라 서비스에서 가져온 PID 출력값을 누적
    //     accum_cam_offset_x += (cam_pid_x * -0.1f);
    //     accum_cam_offset_y += (cam_pid_y * 0.1f);
    // }


    // 1. servo.c에서 정의된 초기값(Init)을 가져옵니다.
    float init_r = servoGetInitAngle(0);
    float init_p = servoGetInitAngle(1);
    float init_y = servoGetInitAngle(2);

    // 이전 목표 각도를 저장하기 위한 정적 변수 (최초 1회만 init값으로 설정)
    static float last_final_roll = -1.0f;
    static float last_final_pitch = -1.0f;
    static float last_final_yaw = -1.0f;

    if (last_final_roll < 0) { // 초기화
        last_final_roll = init_r;
        last_final_pitch = init_p;
        last_final_yaw = init_y;
    }
    
    // 4. 최종 목표 각도 계산 
    // [핵심] 가져온 최신 데이터(imu_r, imu_p)를 사용하고, 방향 보정을 위해 -1.0f를 곱합니다.
    float final_roll  = init_r + (imu_r * -1.0f);
    float final_pitch = init_p + (imu_p * -1.0f);
    float final_yaw   = init_y + 0.0f; //(imu_y * 1.0f)

    // 3. 카메라 제어가 켜져있을 때만 누적값 합산
    // if (is_cam_control_enable) {
    //     final_pitch += accum_cam_offset_y;
    //     final_yaw   += accum_cam_offset_x;
    // }

    // 🌟 4. 카메라 오프셋 적용 (+ 연산이므로 100Hz로 돌아도 누적되지 않음!)
    if (is_cam_control_enable) {
        final_pitch = final_pitch + cam_absolute_y;
        final_yaw   = final_yaw + cam_absolute_x;
    }

    // 4. 서보 모터 하드웨어 보호를 위한 리미트 (Clamping)
   // Roll 축 (CH 0) 리미트 체크
    if (final_roll < servoGetMinAngle(0)) final_roll = servoGetMinAngle(0);
    if (final_roll > servoGetMaxAngle(0)) final_roll = servoGetMaxAngle(0);

    // Pitch 축 (CH 1) 리미트 체크
    if (final_pitch < servoGetMinAngle(1)) final_pitch = servoGetMinAngle(1);
    if (final_pitch > servoGetMaxAngle(1)) final_pitch = servoGetMaxAngle(1);

    // Yaw 축 (CH 2) 리미트 체크
    if (final_yaw < servoGetMinAngle(2)) final_yaw = servoGetMinAngle(2);
    if (final_yaw > servoGetMaxAngle(2)) final_yaw = servoGetMaxAngle(2);

    // [CLI 모니터링] 기존 형식을 유지하되, 숫자 90 부분만 실제 init값으로 출력
    static uint32_t last_log_time = 0;
    if (osKernelGetTickCount() - last_log_time >= 100) {
        last_log_time = osKernelGetTickCount();

        float diff_r = final_roll - last_final_roll;
        float diff_p = final_pitch - last_final_pitch;
        float diff_y = final_yaw - last_final_yaw;

        // 원하신 대로 기존 CLI 포맷을 그대로 유지합니다.
        // R(초기값):현재IMU->최종목표(차이) | P(초기값):현재IMU+오프셋->최종목표(차이) | ...
        // cliPrintf("R(%.1f):%.1f->%.1f(%.2f) | P(%.1f):%.1f+%.1f->%.1f(%.2f) | Y(%.1f):%.1f+%.1f->%.1f(%.2f)\r\n",
        //           init_r, imu_roll, final_roll, diff_r,
        //           init_p, imu_pitch, cam_offset_y, final_pitch, diff_p,
        //           init_y, imu_yaw, cam_offset_x, final_yaw, diff_y);

        // cliPrintf("R(%.1f):%.1f->%.1f(%.2f) | P(%.1f):%.1f+%.1f->%.1f(%.2f) | Y(%.1f):%.1f+%.1f->%.1f(%.2f)\r\n",
        //           init_r, imu_r, final_roll, diff_r,
        //           init_p, imu_p, accum_cam_offset_y, final_pitch, diff_p,
        //           init_y, imu_y, accum_cam_offset_x, final_yaw, diff_y);
        
        cliPrintf("R(%.1f):%.1f->%.1f(%.2f) | P(%.1f):%.1f+%.1f->%.1f(%.2f) | Y(%.1f):%.1f+%.1f->%.1f(%.2f)\r\n",
                  init_r, imu_r, final_roll, diff_r,
                  init_p, imu_p, cam_absolute_y, final_pitch, diff_p,
                  init_y, imu_y, cam_absolute_x, final_yaw, diff_y);
        
        
        // 현재 값을 다음 비교를 위해 저장
        last_final_roll = final_roll;
        last_final_pitch = final_pitch;
        last_final_yaw = final_yaw;
    }

    // 5. 최종 목표 각도를 서보 드라이버에 전달
    //servoSetTarget(0, final_roll,  0.3f); // Roll 축
    //servoSetTarget(1, final_pitch, 0.3f); // Pitch 축
    //servoSetTarget(2, final_yaw,   0.3f); // Yaw 축
    
    servoSetTarget(0, final_roll,  1.0f); // Roll 축
    servoSetTarget(1, final_pitch, 1.0f); // Pitch 축
    servoSetTarget(2, final_yaw,   1.0f); // Yaw 축
    
    // 6. 실제 보간 이동 실행
    servoSmoothUpdate();
}



// /**
//  * @brief 짐벌을 초기 정면 위치로 복귀
//  */
// void gimbalReturnHome(void) {
//     cam_offset_x = 0.0f;
//     cam_offset_y = 0.0f;
//     accum_cam_offset_x = 0.0f; // 누적값 초기화
//     accum_cam_offset_y = 0.0f;

//     servo.c의 설정값을 그대로 가져와서 복귀
//     servoSetTarget(0, servoGetInitAngle(0), 0.1f);
//     servoSetTarget(1, servoGetInitAngle(1), 0.1f);
//     servoSetTarget(2, servoGetInitAngle(2), 0.1f);
// }
void gimbalReturnHome(void) {
    // 🌟 카메라 서비스의 누적값을 리셋 명령
    cameraServiceResetZeroPoint();

    servoSetTarget(0, servoGetInitAngle(0), 0.1f);
    servoSetTarget(1, servoGetInitAngle(1), 0.1f);
    servoSetTarget(2, servoGetInitAngle(2), 0.1f);
}
/**
 * @brief 현재 특정 채널의 서보 각도를 반환
 */
float gimbalGetCurrentAngle(uint8_t ch) {
    
    if (ch >= 3) return 0.0f; // 예외 처리
    return servoGetCurrentAngle(ch);
}



