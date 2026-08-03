/* servo.c */
#include "servo.h"
#include <math.h>

extern TIM_HandleTypeDef htim2; // CubeMX에서 설정한 타이머 핸들러

// 3축 상태 저장소 (외부에서 직접 접근하지 못하도록 static 선언, Getter/Setter로만 접근)
static servo_smooth_t servo_list[3];

/**
 * @brief 서보 초기화: 매크로 초기값을 구조체로 옮겨 담아 동적 제어 준비
 */
void servoInit(void) {
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); 
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); 
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); 

    // 하드웨어 설정값 매크로를 기반으로 초기 배열 생성
    float inits[] = {SERVO_CH0_INIT, SERVO_CH1_INIT, SERVO_CH2_INIT};
    float mins[]  = {SERVO_CH0_MIN,  SERVO_CH1_MIN,  SERVO_CH2_MIN};
    float maxs[]  = {SERVO_CH0_MAX,  SERVO_CH1_MAX,  SERVO_CH2_MAX};

    for(int i=0; i<3; i++) {
        servo_list[i].min_angle     = mins[i];
        servo_list[i].max_angle     = maxs[i];
        servo_list[i].init_angle    = inits[i];
        
        servo_list[i].current_angle = inits[i];
        servo_list[i].target_angle  = inits[i];
        servo_list[i].k             = 0.1f; 
        
        servoWrite(i, (uint8_t)inits[i]);
    }
}

/**
 * @brief PWM 출력 및 동적 임계치 보호
 */
void servoWrite(uint8_t ch, uint8_t angle) {
    if (ch > 2) return;

    // 구조체에 저장된 min/max 값을 참조하여 하드웨어 보호
    if (angle > servo_list[ch].max_angle) angle = (uint8_t)servo_list[ch].max_angle;
    if (angle < servo_list[ch].min_angle) angle = (uint8_t)servo_list[ch].min_angle;

    uint32_t pulse_value = 500 + (angle * (2500 - 500) / 180);

    if (ch == 0)      __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_value);
    else if (ch == 1) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse_value);
    else if (ch == 2) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pulse_value);
}

/**
 * @brief 채널별 동적 Min~Max 범위를 스캔 후 동적 Init(Home) 위치로 복귀
 */
void servoScan(uint8_t ch) {
    if (ch > 2) return;
    cliPrintf("Starting Scan for Channel %d...\r\n", ch);

    // 구조체 범위 참조
    for (int i = (int)servo_list[ch].min_angle; i <= (int)servo_list[ch].max_angle; i += 5) {
        servoWrite(ch, (uint8_t)i);
        cliPrintf("CH %d - Current Angle: %d\r\n", ch, i);
        HAL_Delay(100);
    }
    
    HAL_Delay(500);
    servoWrite(ch, (uint8_t)servo_list[ch].init_angle);
    cliPrintf("Scan Complete. Returned to Init Position (%.1f).\r\n", servo_list[ch].init_angle);
}

/**
 * @brief 듀얼 테스트 후 구조체 Init 값으로 복귀
 */
void servoDualTest(void) {
    cliPrintf("Dual Servo Sync Test Start...\r\n");
    
    for(int i=0; i<=130; i++) { 
        servoWrite(2, i);  
        servoWrite(1, 130-i); 
        if (i % 20 == 0) cliPrintf("Y:%d, P:%d\r\n", i, 130-i);
        HAL_Delay(15); 
    }

    // 구조체의 초기값으로 복귀
    servoWrite(1, (uint8_t)servo_list[1].init_angle);
    servoWrite(2, (uint8_t)servo_list[2].init_angle);
    cliPrintf("Dual Test Done. Returned to Home.\r\n");
}

/**
 * @brief 전체 테스트 완료 후 구조체 Init 위치로 복귀
 */
void servoTotalTest(void) {
    cliPrintf("======= [Gimbal Full Range Test] =======\r\n");

    for (int i = 0; i <= 180; i++) {
        // servoWrite 내부에서 max_angle 초과분은 자동으로 깎아주므로 조건문 불필요
        servoWrite(0, i);
        servoWrite(1, i); 
        servoWrite(2, i);
        if (i % 30 == 0) cliPrintf("Moving... %d deg\r\n", i);
        HAL_Delay(15); 
    }

    HAL_Delay(500);

    cliPrintf("Test Complete. Moving to Home.\r\n");
    servoWrite(0, (uint8_t)servo_list[0].init_angle);
    servoWrite(1, (uint8_t)servo_list[1].init_angle);
    servoWrite(2, (uint8_t)servo_list[2].init_angle);
}

void servoSetTarget(uint8_t ch, float target, float k) {
    if (ch > 2) return;

    // 구조체 제한 범위 확인
    if (target < servo_list[ch].min_angle) target = servo_list[ch].min_angle;
    if (target > servo_list[ch].max_angle) target = servo_list[ch].max_angle;

    servo_list[ch].target_angle = target;
    if (k > 0.0f) servo_list[ch].k = k;
}

void servoSmoothUpdate(void) {
    for (int i = 0; i < 3; i++) {
        float diff = servo_list[i].target_angle - servo_list[i].current_angle;
        if (fabsf(diff) > 0.05f) {
            servo_list[i].current_angle += diff * servo_list[i].k;
            servoWrite(i, (uint8_t)roundf(servo_list[i].current_angle));
        }
    }
}

/**
 * @brief 이미 설정된(Target) 값으로 도달할 때까지 루프를 돌며 이동
 */
void servoRunToTarget(uint8_t ch) {
    if (ch > 2) return;

    while (1) {
        float diff = servo_list[ch].target_angle - servo_list[ch].current_angle;

        if (fabsf(diff) < 0.1f) {
            servo_list[ch].current_angle = servo_list[ch].target_angle;
            servoWrite(ch, (uint8_t)roundf(servo_list[ch].current_angle));
            break;
        }

        servo_list[ch].current_angle += diff * servo_list[ch].k;
        servoWrite(ch, (uint8_t)roundf(servo_list[ch].current_angle));
        HAL_Delay(20);
    }
}

void servoSetTargetAll(float target0, float target1, float target2, float k) {
    servoSetTarget(0, target0, k);
    servoSetTarget(1, target1, k);
    servoSetTarget(2, target2, k);
}

/**
 * @brief 목표 각도에 도달할 때까지 루프를 돌며 부드럽게 이동
 */
void servoMoveToTarget(uint8_t ch, float target, float k) {
    if (ch > 2) return;

    // 하드웨어 보호 및 목표값 설정
    servoSetTarget(ch, target, k);

    while (1) {
        float diff = servo_list[ch].target_angle - servo_list[ch].current_angle;

        if (fabsf(diff) < 0.1f) {
            servo_list[ch].current_angle = servo_list[ch].target_angle;
            servoWrite(ch, (uint8_t)roundf(servo_list[ch].current_angle));
            break;
        }

        servo_list[ch].current_angle += diff * servo_list[ch].k;
        servoWrite(ch, (uint8_t)roundf(servo_list[ch].current_angle));
        HAL_Delay(20); 
    }
}

/**
 * @brief 스위핑 기준을 구조체 값으로 변경
 */
void servoSweep(uint8_t ch, float k) {
    if (ch > 2) return;
    
    float home = servo_list[ch].init_angle;
    float max_l = servo_list[ch].max_angle;
    float min_l = servo_list[ch].min_angle;

    // 현재 위치가 초기값보다 작으면 최대치로, 크면 최소치로 보냄
    float target = (servo_list[ch].current_angle < home) ? max_l : min_l;
    servoSetTarget(ch, target, k);
}


/* ============================================================
 * Getter & Setter 함수 구현부 (다른 파일에서 참조 시 사용)
 * ============================================================ */

// --- MIN ---
void servoSetMinAngle(uint8_t ch, float min) {
    if(ch < 3) servo_list[ch].min_angle = min;
}
float servoGetMinAngle(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].min_angle : 0.0f;
}

// --- MAX ---
void servoSetMaxAngle(uint8_t ch, float max) {
    if(ch < 3) servo_list[ch].max_angle = max;
}
float servoGetMaxAngle(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].max_angle : 180.0f;
}

// --- INIT (Home) ---
void servoSetInitAngle(uint8_t ch, float init) {
    if(ch < 3) servo_list[ch].init_angle = init;
}
float servoGetInitAngle(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].init_angle : 0.0f;
}

// --- CURRENT ---
void servoSetCurrentAngle(uint8_t ch, float angle) {
    if(ch < 3) servo_list[ch].current_angle = angle;
}
float servoGetCurrentAngle(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].current_angle : 0.0f;
}

// --- TARGET ---
// 타겟은 servoSetTarget()으로 설정하므로 Getter만 제공
float servoGetTargetAngle(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].target_angle : 0.0f;
}

// --- K (Smoothing Factor) ---
void servoSetK(uint8_t ch, float k) {
    // k값이 너무 작아져 멈추는 현상 방지
    if(ch < 3 && k > 0.0f) servo_list[ch].k = k;
}
float servoGetK(uint8_t ch) {
    return (ch < 3) ? servo_list[ch].k : 0.1f;
}