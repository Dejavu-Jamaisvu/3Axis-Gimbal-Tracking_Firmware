/* gyro_service.h */

#ifndef __AP_SERVICE_GIMBAL_H__
#define __AP_SERVICE_GIMBAL_H__


// #include "gimbal_control.h" // gimbalUpdateFromIMU 보고용

#include "monitor.h"

// gimbal_control.h 안에 있음 
// #include "hw_def.h"
// #include "cli.h"            // cliPrintf, isMonitoringOn 사용
// #include "gyro.h"   // RAD_TO_DEG, GYRO_SCALE 참조, Gyro_Read, Gyro_Data_t 사용
// #include "servo.h"  // SERVO_CH2_INIT 참조
// #include "mag.h" // Mag_Data_t 정의를 위해 필요

// [수정] 하드코딩 방지를 위해 서비스용 가중치를 헤더에서 관리
#define IMU_ALPHA        0.96f   // Roll, Pitch용
#define YAW_COMP_ALPHA   0.94f   // Yaw 상보필터용



// 서비스 초기화 (MPU6050 초기화 포함)
bool gyroServiceInit(void);
void gyroServiceUpdate(void);
bool gyroServiceReInit(void);
bool gyroServiceIsOk(void);


// CLI 등에서 사용할 설정 함수
void gyroServiceSetPeriod(uint32_t period);
void gyroServiceSetMagData(Mag_Data_t *p_mag);


/**
 * @brief [추가] 현재 절대 각도를 서보의 초기 위치(INIT)와 매칭시키는 기준점으로 설정
 */
void gyroServiceSetOrigin(float current_abs_yaw);

void gyroServiceSyncCurrentOrigin(void);

// [수정: 제어기가 데이터를 가져갈 수 있도록 Getter 추가]
void gyroServiceGetLatestAngles(float *r, float *p, float *y);

#endif