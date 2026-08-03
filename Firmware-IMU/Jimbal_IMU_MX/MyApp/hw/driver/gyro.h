#ifndef GYRO_H
#define GYRO_H

#include "def.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdbool.h>
#include "mag.h" // 지자기 데이터 구조체를 쓰기 위해 인클루드

#ifndef RAD_TO_DEG
#define RAD_TO_DEG 57.29577951f
#endif
#ifndef GYRO_SCALE
#define GYRO_SCALE 131.0f
#endif


// IMU 데이터 및 계산된 각도 저장 구조체
typedef struct {
    // raw 데이터
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    // 계산된 각도 (Roll, Pitch, Yaw)
    float roll, pitch, yaw; // 최종 출력 각도 (단위: Degree)
} Gyro_Data_t;
    
   

bool Gyro_Init(void);
void Gyro_StartAuto(void);
void Gyro_StopAuto(void);
bool Gyro_Read(Gyro_Data_t *pData);

// 각도 계산 함수 (상호보완 필터 적용)
void IMU_ComputeAngles(Gyro_Data_t *imu, Mag_Data_t *mag, float dt);

#endif // GYRO_H

