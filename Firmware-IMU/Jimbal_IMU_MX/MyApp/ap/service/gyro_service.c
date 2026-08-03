/* gyro_service.c */

#include "gyro_service.h"



/* --- 내부 정적 변수 --- */
static bool is_gyro_ok = false;
static uint32_t imu_report_period = 0;
static Mag_Data_t current_magData; // 지자기 데이터 저장용
static uint32_t last_calc_tick = 0;
static uint32_t last_print_tick = 0;
extern osSemaphoreId_t GyroReadySemHandle;
static uint32_t   mag_updated_tick = 0; // 데이터 신선도 체크용

// mag
static float yaw_origin = 0.0f;
static bool is_origin_set = false;
static float filtered_rel_yaw = 0.0f; // 정적 변수로 유지하여 필터 값 보존

// [수정: 최신 계산값을 저장할 내부 변수 추가]
static float latest_roll = 0.0f;
static float latest_pitch = 0.0f;
static float latest_yaw = 0.0f;
/**
 * @brief [수정] 전원 켠 순간의 절대 각도를 영점으로 잡는 함수 분리
 */
void gyroServiceSetOrigin(float current_abs_yaw) {
    yaw_origin = current_abs_yaw;
    is_origin_set = true;
    filtered_rel_yaw = 0.0f; 
    cliPrintf("[IMU] New Yaw Origin Set: %.2f\r\n", yaw_origin);
}

/* gyro_service.c */

void gyroServiceSyncCurrentOrigin(void) {
    // 1. 현재 지자기 센서가 읽고 있는 절대 방위각을 가져옴
    float current_abs_yaw = latest_yaw + yaw_origin;
    
    // 2. 이 각도를 새로운 영점(yaw_origin)으로 설정
    yaw_origin = current_abs_yaw;
    is_origin_set = true;
    
    // 3. 자이로 적분으로 계산 중이던 상대 각도도 0으로 리셋
    filtered_rel_yaw = 0.0f;// 상대 각도 누적값 리셋
    latest_yaw = 0.0f; //  추가: 제어기가 가져가는 최신값도 즉시 0으로!
    
    cliPrintf("[IMU] Zero-Point Aligned to Current Heading.\r\n");}

// /**
//  * @brief 센서 데이터를 읽어 각도를 계산하고 제어기로 전송
//  */
// static void IMU_ComputeAngles_Service(Gyro_Data_t *imu, Mag_Data_t *mag, float dt) {
//     if (mag == NULL) return;

//     // 1. Roll/Pitch 계산 (gyro.h의 SCALE 및 RAD_TO_DEG 참조)
//     float acc_roll = atan2f((float)imu->accel_y, (float)imu->accel_z) * RAD_TO_DEG;
//     float acc_pitch = atan2f(-(float)imu->accel_x, 
//                       sqrtf((float)imu->accel_y * imu->accel_y + (float)imu->accel_z * imu->accel_z)) * RAD_TO_DEG;

//     float gyro_rate_x = (float)imu->gyro_x / GYRO_SCALE;
//     float gyro_rate_y = (float)imu->gyro_y / GYRO_SCALE;
//     float gyro_rate_z = (float)imu->gyro_z / GYRO_SCALE;

//     // IMU_ALPHA는 서비스 헤더 혹은 상단에 정의
//     imu->roll  = IMU_ALPHA * (imu->roll + gyro_rate_x * dt) + (1.0f - IMU_ALPHA) * acc_roll;
//     imu->pitch = IMU_ALPHA * (imu->pitch + gyro_rate_y * dt) + (1.0f - IMU_ALPHA) * acc_pitch;

//     // 2. Yaw 연산
//     float current_abs_yaw = atan2f(-mag_y_comp, mag_x_comp) * RAD_TO_DEG;
//     if (current_abs_yaw < 0) current_abs_yaw += 360.0f;

//     // 3. [수정] 전원 켠 순간 혹은 필요 시 영점 설정
//     if (!is_origin_set) {
//         gyroServiceSetOrigin(current_abs_yaw);
//     }

//     // 4. 상대 각도 계산 및 보정
//     float relative_yaw = current_abs_yaw - yaw_origin;
//     if (relative_yaw > 180.0f)  relative_yaw -= 360.0f;
//     if (relative_yaw < -180.0f) relative_yaw += 360.0f;

//     static float filtered_rel_yaw = 0.0f;
//     filtered_rel_yaw = YAW_COMP_ALPHA * (filtered_rel_yaw + gyro_rate_z * dt) + (1.0f - YAW_COMP_ALPHA) * relative_yaw;

//     // 5. [수정] 하드코딩 90.0f 대신 servo.h의 INIT 값을 기준으로 매핑
//     imu->yaw = SERVO_CH2_INIT + filtered_rel_yaw;

//     // 데이터 전송
//     gimbalSettingIMU(imu->roll, imu->pitch, imu->yaw);
// }

/**
 * @brief 센서 데이터를 읽어 각도를 계산하고 제어기로 전송
 * [최종 수정] 
 * 1. Positive Feedback 방지를 위해 Yaw 부호 반전
 * 2. 지자기 데이터 부재 시 자이로 적분 유지 (끊김 방지)
 * 3. 시스템 멈춤 방지를 위해 함수 내 모든 printf 제거
 */
static void IMU_ComputeAngles_Service(Gyro_Data_t *imu, Mag_Data_t *mag, float dt) {
    // --- 1. Roll/Pitch 계산 (가속도 + 자이로 상보필터) ---
    float acc_roll = atan2f((float)imu->accel_y, (float)imu->accel_z) * RAD_TO_DEG;
    float acc_pitch = atan2f(-(float)imu->accel_x, 
                      sqrtf((float)imu->accel_y * imu->accel_y + (float)imu->accel_z * imu->accel_z)) * RAD_TO_DEG;

    float gyro_rate_x = (float)imu->gyro_x / GYRO_SCALE;
    float gyro_rate_y = (float)imu->gyro_y / GYRO_SCALE;
    float gyro_rate_z = (float)imu->gyro_z / GYRO_SCALE;

    imu->roll  = IMU_ALPHA * (imu->roll + gyro_rate_x * dt) + (1.0f - IMU_ALPHA) * acc_roll;
    imu->pitch = IMU_ALPHA * (imu->pitch + gyro_rate_y * dt) + (1.0f - IMU_ALPHA) * acc_pitch;

    // --- 2. Yaw 연산 (지자기 기반 + 기울기 보정) ---
    if (mag != NULL) {
        float roll_rad = imu->roll / RAD_TO_DEG;
        float pitch_rad = imu->pitch / RAD_TO_DEG;

        // 기울기 보정 (Tilt Compensation)
        float mag_x_comp = mag->mag_x * cosf(pitch_rad) + mag->mag_z * sinf(pitch_rad);
        float mag_y_comp = mag->mag_x * sinf(roll_rad) * sinf(pitch_rad) + 
                           mag->mag_y * cosf(roll_rad) - 
                           mag->mag_z * sinf(roll_rad) * cosf(pitch_rad);

        // [안전장치] atan2f 입력값이 모두 0인 경우 연산 스킵 (시스템 정지 방지)
        if (fabsf(mag_x_comp) < 0.0001f && fabsf(mag_y_comp) < 0.0001f) return;

        float current_abs_yaw = atan2f(-mag_y_comp, mag_x_comp) * RAD_TO_DEG;
        if (current_abs_yaw < 0) current_abs_yaw += 360.0f;

        // [영점 설정] 최초 실행 시 현재 각도를 영점으로 고정
        if (!is_origin_set) {
            // 초기 0.0 데이터가 아닌 유효한 데이터일 때만 영점 잡기
            if (fabsf(current_abs_yaw) > 0.1f) {
                gyroServiceSetOrigin(current_abs_yaw);
            }
        }

        // 상대 각도 계산 및 -180 ~ 180 범위 보정
        float relative_yaw = current_abs_yaw - yaw_origin;
        if (relative_yaw > 180.0f)  relative_yaw -= 360.0f;
        if (relative_yaw < -180.0f) relative_yaw += 360.0f;

        // [중요] 상보 필터 적용
        // 자이로 적분값에 지자기 보정치를 아주 조금씩 섞어 흐름(Drift)을 방지합니다.
        filtered_rel_yaw = YAW_COMP_ALPHA * (filtered_rel_yaw + gyro_rate_z * dt) + (1.0f - YAW_COMP_ALPHA) * relative_yaw;

    } else {
        // [수정] 지자기 데이터가 없는 동안(NULL)에도 자이로는 계속 적분해야 멈추지 않습니다.
        // 만약 여기서 0.0f를 넣어버리면 짐벌이 갑자기 툭 하고 원점으로 돌아가려 합니다.
        filtered_rel_yaw += gyro_rate_z * dt;
    }

    /* * [핵심 수정: 왼쪽 쏠림 방지] 
     * 기체가 왼쪽(+)으로 돌면, 서보는 오른쪽(-)으로 움직여야 정면을 유지합니다.
     * 현재 짐벌이 한쪽으로 계속 돌아간다면 아래와 같이 부호를 반전시켜야 합니다.
     */
    imu->yaw = -filtered_rel_yaw; 

    // [수정: gimbalSettingIMU 호출 대신 내부 변수에 저장]
    latest_roll  = imu->roll;
    latest_pitch = imu->pitch;
    latest_yaw   = -filtered_rel_yaw; // 부호 반전 포함

}
//////////////////////////////


void gyroServiceSetMagData(Mag_Data_t *p_mag) {
    if (p_mag == NULL) return;
    
    // Mutex가 있다면 여기서 Lock
    current_magData = *p_mag;
    mag_updated_tick = osKernelGetTickCount();
    // Mutex Unlock
}

/**
 * @brief 서비스 초기화
 */
bool gyroServiceInit(void) {

    
    is_origin_set = false;  // 반드시 명시적으로 false 처리
    yaw_origin = 0.0f;
    filtered_rel_yaw = 0.0f;

    is_gyro_ok = Gyro_Init();
    last_calc_tick = osKernelGetTickCount();

    // 이 로그가 CLI에 찍히는지 확인하세요.
    cliPrintf("[IMU] Service Initialized. Origin Reset Pending...\r\n");
    return is_gyro_ok;

    
}


// 외부에서 상태를 물어볼 때 답해주는 함수
bool gyroServiceIsOk(void) {
    return is_gyro_ok;
}

/**
 * @brief 하드웨어 재시도 로직 (cli imu on 시 호출)
 */
bool gyroServiceReInit(void) {
    cliPrintf("Re-initializing I2C & Sensors...\r\n");
    is_gyro_ok = Gyro_Init();
    return is_gyro_ok;
}
/**
 * @brief 리포트 주기 및 센서 동작 설정
 */
void gyroServiceSetPeriod(uint32_t period) {
    imu_report_period = period;
    if (period > 0) {
        Gyro_StartAuto();
    } else {
        Gyro_StopAuto();
    }
}


// /**
//  * @brief 핵심 로직: 센서 읽기 -> 각도 계산 -> 짐벌 전달 -> 리포팅
//  */
// void gyroServiceUpdate(void) {
//     if (!is_gyro_ok || GyroReadySemHandle == NULL) {
//         return;
//     }

//     // 세마포어 획득 (센서 데이터 준비 완료 대기)
//     if (osSemaphoreAcquire(GyroReadySemHandle, osWaitForever) != osOK) return;

//     static Gyro_Data_t gyroData;

//     // 1. 센서 데이터 읽기 및 연결 확인
//     if (!Gyro_Read(&gyroData)) {
//         is_gyro_ok = false;
//         cliPrintf("\r\n[ALARM] Sensor Disconnected! Type 'imu off' then 'imu on' to recover.\r\n");
//         return;
//     }

//     // 2. 시간 간격(dt) 계산
//     uint32_t now = osKernelGetTickCount();
//     float dt = (now - last_calc_tick) / 1000.0f;
//     last_calc_tick = now;


//     // 데이터 신선도 확인 (예: 500ms 이상 갱신 안 되면 가짜 데이터나 무시)
//     Mag_Data_t *p_mag_to_use = &current_magData;
//     if (osKernelGetTickCount() - mag_updated_tick > 500) {
//         p_mag_to_use = NULL; // IMU_ComputeAngles 내부에서 NULL 체크를 하도록 설계
//     }


//     // 3. 센서 융합 및 각도 계산 (가속도 + 자이로 + 지자기)
//     // IMU_ComputeAngles(&gyroData, &current_magData, dt);
//     IMU_ComputeAngles(&gyroData, p_mag_to_use, dt); // NULL 처리가 반영된 포인터를 넘겨야 함


//     // 4. 계산된 각도를 짐벌 제어기로 즉시 전송 (가장 중요)
//     gimbalSettingIMU(gyroData.roll, gyroData.pitch, gyroData.yaw);

//     // 5. 주기적 출력 제어 (CLI 또는 모니터링 툴)
//     if (imu_report_period > 0 && (now - last_print_tick >= imu_report_period)) {
//         last_print_tick = now;

//         if (!isMonitoringOn()) {
//             cliPrintf("Angle [Roll: %6.1f | Pitch: %6.1f | Yaw: %6.1f]\r\n", 
//                       gyroData.roll, gyroData.pitch, gyroData.yaw);
//         } else {
//             monitorUpdateValue(ID_IMU_GYRO_X, TYPE_FLOAT, &gyroData.roll);
//             monitorUpdateValue(ID_IMU_GYRO_Y, TYPE_FLOAT, &gyroData.pitch);
//             monitorUpdateValue(ID_IMU_GYRO_Z, TYPE_FLOAT, &gyroData.yaw);
//         }
//     }
// }

/**
 * @brief 핵심 로직 루프
 */
void gyroServiceUpdate(void) {
    if (!is_gyro_ok || GyroReadySemHandle == NULL) return;

    if (osSemaphoreAcquire(GyroReadySemHandle, osWaitForever) != osOK) return;

    static Gyro_Data_t gyroData;

    if (!Gyro_Read(&gyroData)) {
        is_gyro_ok = false;
        cliPrintf("\r\n[ALARM] Sensor Disconnected!\r\n");
        return;
    }

    uint32_t now = osKernelGetTickCount();
    float dt = (now - last_calc_tick) / 1000.0f;
    if (dt <= 0.0f) dt = 0.01f;
    last_calc_tick = now;

    // 데이터 신선도 확인 (500ms)
    Mag_Data_t *p_mag_to_use = &current_magData;
    if (osKernelGetTickCount() - mag_updated_tick > 500) {
        p_mag_to_use = NULL; 
    }

    // [수정] 아래의 내부 서비스 함수를 호출해야 합니다.
    IMU_ComputeAngles_Service(&gyroData, p_mag_to_use, dt);

    // 5. 주기적 출력 리포팅
    if (imu_report_period > 0 && (now - last_print_tick >= imu_report_period)) {
        last_print_tick = now;
        if (!isMonitoringOn()) {
            cliPrintf("Angle [R: %6.1f | P: %6.1f | Y: %6.1f]\r\n", 
                      gyroData.roll, gyroData.pitch, gyroData.yaw);
        } else {
            monitorUpdateValue(ID_IMU_GYRO_X, TYPE_FLOAT, &gyroData.roll);
            monitorUpdateValue(ID_IMU_GYRO_Y, TYPE_FLOAT, &gyroData.pitch);
            monitorUpdateValue(ID_IMU_GYRO_Z, TYPE_FLOAT, &gyroData.yaw);
        }
    }
}

// [수정: Pull 방식용 Getter 함수 구현]
void gyroServiceGetLatestAngles(float *r, float *p, float *y) {
    if (r) *r = latest_roll;
    if (p) *p = latest_pitch;
    if (y) *y = latest_yaw;
}