#include "ap.h"
#include "lcd.h"
#include "uart.h"

// 변수 초기화
uint16_t targetX = 110;
uint16_t targetY = 150;
uint16_t boxSize = 20;

void apInit(void) {
    uartInit();
    LCD_Init();         // 1. LCD 초기 설정 (명령어 전송)
    
    HAL_Delay(10);      // 초기화 후 아주 잠깐 대기
    
    LCD_FillScreen(BLUE); // 2. 전체 배경을 파란색으로 채움
    
    HAL_Delay(10);      // 배경 채운 후 대기
    
    // 3. 테스트용: 빨간 네모를 정중앙에 고정해서 그리기
    // 입력 없이도 이 코드가 실행되면 화면에 바로 떠야 합니다.
    LCD_DrawRect(targetX, targetY, boxSize, boxSize, RED);
}

void apMain(void) {
    // 키보드 조종 로직은 일단 주석 처리하거나 비워둡니다.
    // 화면에 네모가 뜨는지 확인하는 것이 우선입니다!
    
    /*
    if (uartAvailable(0) > 0) 
    {
        // ... (생략) ...
    }
    */
    
    osDelay(10); // 시스템 안정성을 위해 딜레이 유지
}