#ifndef __GIMBAL_CONTROL_H__
#define __GIMBAL_CONTROL_H__

#include "hw_def.h"
#include "servo.h"          // servoSetTarget, servoGetCurrentAngle 정의 포함
#include "gyro_service.h"
#include "camera_service.h"

// 짐벌 제어 채널 정의
#define GIMBAL_CH_ROLL   0
#define GIMBAL_CH_PITCH  1
#define GIMBAL_CH_YAW    2

/* 서비스 간 데이터 공유를 위한 인터페이스 */

// /**
//  * @brief 자이로 서비스가 계산된 각도를 전달하기 위해 호출
//  */
// void gimbalSettingIMU(float roll, float pitch, float yaw);

// /**
//  * @brief 카메라 서비스가 계산된 PID 오차를 전달하기 위해 호출
//  */
// void gimbalSettingCamOffset(float x, float y);

/* 메인 로직 인터페이스 (ap.c에서 사용) */

/**
 * @brief 수집된 모든 데이터를 합산하여 실제 서보 명령을 실행 (10ms 주기 권장)
 */
void gimbalExecuteCombinedControl(void);

/**
 * @brief 초기 위치(90도) 및 오프셋 초기화
 */
void gimbalReturnHome(void);

/**
 * @brief 특정 채널의 현재 서보 각도 획득
 */
float gimbalGetCurrentAngle(uint8_t ch);


/**
 * @brief 카메라 제어 활성화/비활성화 설정
 */
void gimbalSetCamControlEnable(bool enable);

/**
 * @brief 현재 카메라 제어 활성화 상태 확인
 */
bool gimbalGetCamControlEnable(void);

#endif /* __GIMBAL_CONTROL_H__ */