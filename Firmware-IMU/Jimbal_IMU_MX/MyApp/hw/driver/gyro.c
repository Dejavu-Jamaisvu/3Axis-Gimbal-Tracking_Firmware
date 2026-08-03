#include "gyro.h"


extern I2C_HandleTypeDef hi2c1;
#define MPU6050_ADDR (0x68 << 1)

#define RAD_TO_DEG 57.29577951f
#define DEG_TO_RAD 0.017453293f
#define GYRO_SCALE 131.0f  // MPU6050 FS_SEL=0 일 때의 Scale Factor
#define ALPHA      0.96f   // 상호보완 필터 가중치 (자이로 96%, 가속도 4%)
// 센서 감도 상수 (MPU6050 기본설정 기준)
#define ACCEL_SENS      16384.0f     // +/- 2g 범위
#define GYRO_SENS       131.0f       // +/- 250dps 범위


bool Gyro_Init(void) {
    uint8_t check;
    uint8_t data;

    // 0. 강제 하드웨어 리셋 (전원을 껐다 켜야만 되는 현상 방지)
    data = 0x80;
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x6B, 1, &data, 1, 100);
    osDelay(100); // 리셋 대기 시간

    // 센서 연결 확인
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, 0x75, 1, &check, 1, 100) != HAL_OK) {
        return false;
    }

    if (check == 0x68) {
        // 1. 절전 모드 해제 (클럭 소스 PLL 지정)
        data = 0x01;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x6B, 1, &data, 1, 100);
        
        // 2. 노이즈 필터 (DLPF) 적용 -> 기본 1kHz 샘플링 설정
        data = 0x03; // 약 40Hz 필터 적용
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x1A, 1, &data, 1, 100);

        // 3. 샘플링 속도(Sample Rate) 50Hz로 낮추기 (CLI 먹통 방지)
        // 1kHz / (19 + 1) = 50Hz (20ms 간격으로 인터럽트 발생)
        data = 19;
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x19, 1, &data, 1, 100);

        // 4. 인터럽트 핀 설정 (Latch enable, Clear on any read)
        // 데이터를 완벽히 읽을 때까지 INT 핀을 HIGH로 묶어둠 (노이즈 방지)
        data = 0x30; 
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x37, 1, &data, 1, 100);

        data = 0x00; 
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x1B, 1, &data, 1, 100); // Gyro 250dps
       
        data = 0x00; 
        HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x1C, 1, &data, 1, 100); // Accel 2g

        return true;
    }
    return false;
}

void Gyro_StartAuto(void) {
    uint8_t data = 0x01; // Data Ready 인터럽트 활성화
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x38, 1, &data, 1, 100);
}

void Gyro_StopAuto(void) {
    uint8_t data = 0x00; // 인터럽트 비활성화
    HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, 0x38, 1, &data, 1, 100);
}



static uint32_t last_time = 0;

bool Gyro_Read(Gyro_Data_t *pData) {
    uint8_t raw[14];
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, 0x3B, 1, raw, 14, 10) != HAL_OK) return false;

    // 1. Raw 데이터 가공 (Little Endian 조합)
    pData->accel_x = (int16_t)(raw[0] << 8 | raw[1]);
    pData->accel_y = (int16_t)(raw[2] << 8 | raw[3]);
    pData->accel_z = (int16_t)(raw[4] << 8 | raw[5]);
    pData->gyro_x  = (int16_t)(raw[8] << 8 | raw[9]);
    pData->gyro_y  = (int16_t)(raw[10] << 8 | raw[11]);
    pData->gyro_z  = (int16_t)(raw[12] << 8 | raw[13]);

    // 2. 시간 변화량(dt) 계산
    uint32_t now = HAL_GetTick();
    float dt = (now - last_time) / 1000.0f;
    last_time = now;

    // 3. 가속도 기준 각도 계산 (atan2 사용으로 모든 범위 커버)
    // 연산 최적화: 가속도 raw값을 중력 단위(g)로 변환하지 않고 직접 비율로 계산
    float acc_roll  = atan2f((float)pData->accel_y, (float)pData->accel_z) * RAD_TO_DEG;
    float acc_pitch = atan2f(-(float)pData->accel_x, sqrtf((float)pData->accel_y * pData->accel_y + (float)pData->accel_z * pData->accel_z)) * RAD_TO_DEG;

    // // 4. 상보 필터 (Complementary Filter)
    // // 자이로의 빠른 반응성 + 가속도의 절대 위치 결합
    // // 연산속도 1순위: 복잡한 칼만 필터 대신 실수 곱셈/덧셈으로만 구성
    // pData->roll  = ALPHA * (pData->roll  + (pData->gyro_x / GYRO_SENS) * dt) + (1.0f - ALPHA) * acc_roll;
    // pData->pitch = ALPHA * (pData->pitch + (pData->gyro_y / GYRO_SENS) * dt) + (1.0f - ALPHA) * acc_pitch;

    return true;
}

// void IMU_ComputeAngles(Gyro_Data_t *imu, Mag_Data_t *mag, float dt) {
//     // 1. 가속도 센서로 Roll, Pitch 절대 각도 계산
//     // atan2f와 sqrtf는 STM32F411 하드웨어 부동소수점 연산기로 1클럭만에 처리됨
//     float accel_roll  = atan2f((float)imu->accel_y, (float)imu->accel_z) * RAD_TO_DEG;
//     float accel_pitch = atan2f(-(float)imu->accel_x, 
//                         sqrtf((float)imu->accel_y * imu->accel_y + (float)imu->accel_z * imu->accel_z)) * RAD_TO_DEG;

//     // 2. 자이로 센서로 각속도(Degree per second) 계산
//     float gyro_rate_roll  = (float)imu->gyro_x / GYRO_SCALE;
//     float gyro_rate_pitch = (float)imu->gyro_y / GYRO_SCALE;
//     float gyro_rate_yaw   = (float)imu->gyro_z / GYRO_SCALE;

//     // 3. 상호보완 필터 (Complementary Filter)
//     // 자이로의 빠른 반응성과 가속도의 절대적인 수평 기준을 섞음 (가장 빠르고 메모리가 적은 방법)
//     imu->roll  = ALPHA * (imu->roll + gyro_rate_roll * dt) + (1.0f - ALPHA) * accel_roll;
//     imu->pitch = ALPHA * (imu->pitch + gyro_rate_pitch * dt) + (1.0f - ALPHA) * accel_pitch;

//     // 4. 기울기 보상 지자기 (Tilt-Compensated Yaw) 계산
//     // 평평하지 않은 상태에서도 지자기 센서가 정확한 북쪽을 가리키게 보정
//     float roll_rad  = imu->roll * DEG_TO_RAD;
//     float pitch_rad = imu->pitch * DEG_TO_RAD;

//     float cos_r = cosf(roll_rad);
//     float sin_r = sinf(roll_rad);
//     float cos_p = cosf(pitch_rad);
//     float sin_p = sinf(pitch_rad);

//     // HMC5883L 데이터 변환 (자기장 보정)
//     float mag_x = (float)mag->mag_x;
//     float mag_y = (float)mag->mag_y;
//     float mag_z = (float)mag->mag_z;

//     float mag_x_comp = mag_x * cos_p + mag_z * sin_p;
//     float mag_y_comp = mag_x * sin_r * sin_p + mag_y * cos_r - mag_z * sin_r * cos_p;

//     // 최종 Yaw(방위각) 계산 (0~360도 또는 -180~180도)
//     float yaw_raw = atan2f(-mag_y_comp, mag_x_comp) * RAD_TO_DEG;
    
//     // (선택) Yaw 값에도 약간의 자이로를 섞어주면 흔들림(노이즈)이 줄어듭니다.
//     // imu->yaw = 0.90f * (imu->yaw + gyro_rate_yaw * dt) + 0.10f * yaw_raw;
//     imu->yaw =  yaw_raw; //+ 180.0f; // 

// }