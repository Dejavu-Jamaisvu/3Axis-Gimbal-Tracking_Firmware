/* servo.h */
#ifndef __SERVO_H__
#define __SERVO_H__

#include "hw_def.h"
#include "cli.h"

/**
 * [하드웨어 초기 설정값]
 */
#define SERVO_CH0_MIN    0
#define SERVO_CH0_INIT   110.0f
#define SERVO_CH0_MAX    180

#define SERVO_CH1_MIN    0
#define SERVO_CH1_INIT   65.0f
#define SERVO_CH1_MAX    130

#define SERVO_CH2_MIN    0
#define SERVO_CH2_INIT   70.0f
#define SERVO_CH2_MAX    180

typedef struct {
    float current_angle; // 현재 각도
    float target_angle;  // 목표 각도
    float k;             // 부드러움 계수 (속도)
    float min_angle;     // 최소 각도 제한
    float max_angle;     // 최대 각도 제한
    float init_angle;    // 초기(Home) 위치
} servo_smooth_t;

/* 핵심 제어 함수 */
void servoInit(void);
void servoWrite(uint8_t ch, uint8_t angle);
void servoSetTarget(uint8_t ch, float target, float k);
void servoSetTargetAll(float target0, float target1, float target2, float k);
void servoSmoothUpdate(void);
void servoRunToTarget(uint8_t ch);
void servoMoveToTarget(uint8_t ch, float target, float k);

/* 테스트 함수 */
void servoScan(uint8_t ch);
void servoDualTest(void);
void servoTotalTest(void);
void servoSweep(uint8_t ch, float k);

/* ============================================================
 * Getter & Setter (다른 파일에서 이 함수들을 통해 설정값 참조)
 * ============================================================ */

// Min Angle
void  servoSetMinAngle(uint8_t ch, float min);
float servoGetMinAngle(uint8_t ch);

// Max Angle
void  servoSetMaxAngle(uint8_t ch, float max);
float servoGetMaxAngle(uint8_t ch);

// Init (Home) Angle
void  servoSetInitAngle(uint8_t ch, float init);
float servoGetInitAngle(uint8_t ch);

// Current Angle
void  servoSetCurrentAngle(uint8_t ch, float angle);
float servoGetCurrentAngle(uint8_t ch);

// Target Angle
float servoGetTargetAngle(uint8_t ch);

// K (Smoothing Factor)
void  servoSetK(uint8_t ch, float k);
float servoGetK(uint8_t ch);

#endif