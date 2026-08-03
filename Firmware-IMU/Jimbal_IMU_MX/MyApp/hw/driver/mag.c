#include "mag.h"
#include "stm32f4xx_hal.h"
#include <math.h>
#include "cli.h"    // 로그 출력을 위해 추가 (cliPrintf 사용)

#define RAD_TO_DEG 57.29577951f

extern I2C_HandleTypeDef hi2c1;
#define HMC5883L_ADDR (0x1E << 1)

// 로그 출력 주기를 관리하기 위한 정적 변수
static uint32_t last_log_time = 0;

// bool Mag_Init(void) {
//     uint8_t data = 0x70; // 8-average, 15Hz
//     if (HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x00, 1, &data, 1, 100) != HAL_OK) return false;
    
//     data = 0x03; // 초기엔 Idle 모드 (전력소모 방지)
//     HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x02, 1, &data, 1, 100);
//     return true;
// }
bool Mag_Init(void) {
    uint8_t data;

    // 1. 샘플링 속도 및 평균 설정
    data = 0x70; 
    if (HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x00, 1, &data, 1, 100) != HAL_OK) return false;
    
    // 2. Gain 설정 추가 (0.92mG/Lsb) - 이 줄이 없으면 값이 잘 안 변할 수 있음
    data = 0x20; 
    HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x01, 1, &data, 1, 100);
    
    // 3. 측정 모드 (처음부터 Continuous로 시작하거나 Idle 유지)
    data = 0x00; // 바로 측정 시작하려면 0x00
    HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x02, 1, &data, 1, 100);
    return true;
}

void Mag_StartAuto(void) {
    uint8_t data = 0x00; // Continuous 측정 모드
    HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x02, 1, &data, 1, 100);
}

void Mag_StopAuto(void) {
    uint8_t data = 0x03; // Idle 모드
    HAL_I2C_Mem_Write(&hi2c1, HMC5883L_ADDR, 0x02, 1, &data, 1, 100);
}

float Mag_GetYaw(Mag_Data_t *pMag) {
    // 단순 Yaw 계산 (수평 상태 가정)
    // 실제로는 Roll/Pitch를 이용한 Tilt Compensation이 필요하나 속도를 위해 기본형 제공
    float yaw = atan2f((float)pMag->mag_y, (float)pMag->mag_x) * RAD_TO_DEG;
    if(yaw < 0) yaw += 360.0f;
    return yaw;
}


// bool Mag_Read(Mag_Data_t *pData) {
//     uint8_t raw[6];
//     if (HAL_I2C_Mem_Read(&hi2c1, HMC5883L_ADDR, 0x03, 1, raw, 6, 10) != HAL_OK) return false;
    
//     pData->mag_x = (int16_t)(raw[0] << 8 | raw[1]);
//     pData->mag_z = (int16_t)(raw[2] << 8 | raw[3]);
//     pData->mag_y = (int16_t)(raw[4] << 8 | raw[5]);

// // --- 로그 출력 부 ---
//     // 100ms마다 한 번씩 RAW 데이터를 출력합니다.
//     if (HAL_GetTick() - last_log_time >= 100) {
//         last_log_time = HAL_GetTick();
        
//         // 현재 Yaw 각도를 미리 계산해서 로그에 같이 띄워줍니다.
//         float current_yaw = Mag_GetYaw(pData);
        
//         cliPrintf("[MAG] X:%d | Y:%d | Z:%d | Yaw:%.1f\r\n", 
//                   pData->mag_x, pData->mag_y, pData->mag_z, current_yaw);
//     }



//     return true;
// }
bool Mag_Read(Mag_Data_t *pData) {
    uint8_t raw[6];
    if (HAL_I2C_Mem_Read(&hi2c1, HMC5883L_ADDR, 0x03, 1, raw, 6, 50) != HAL_OK) return false;
    
    int16_t raw_x = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t raw_z = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t raw_y = (int16_t)((raw[4] << 8) | raw[5]);

    // [최종 보정값 적용]
    // 분석된 데이터 기반: X 중심점 14, Y 중심점 36
    pData->mag_x = raw_x - 14; 
    pData->mag_y = raw_y - 36; 
    pData->mag_z = raw_z;

    if (HAL_GetTick() - last_log_time >= 100) {
        last_log_time = HAL_GetTick();
        float current_yaw = Mag_GetYaw(pData);
        // 이제 보정된 X, Y와 함께 정확한 Yaw가 출력됩니다.
        cliPrintf("[MAG] X:%d | Y:%d | Yaw:%.1f\r\n", 
                  pData->mag_x, pData->mag_y, current_yaw);
    }
    return true;
}