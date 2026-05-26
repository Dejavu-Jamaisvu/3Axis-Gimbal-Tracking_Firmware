/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// 함수 포인터 (점프를 위해 사용)
typedef void (*pFunction)(void);

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 메모리 주소 정의 (앞서 설명한 메모리 지도 참고)
#define OTA_FLAG_ADDRESS        0x08004000 // Sector 1 부트로더 실행 코드
#define APPLICATION_ADDRESS       0x08008000 // Sector 2 OTA 업데이트 상태 깃발 (Flag) 저장소
#define DOWNLOAD_PARTITION_ADDR        0x08040000 // Sector 6 실제 짐벌 앱 (Main App) 실행 영역
#define OTA_UPDATE_MAGIC_NUM    0xDEADBEEF // 업데이트 필요 플래그
#define APP_MAX_SIZE            (224 * 1024) // 224KB


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
void Perform_OTA_Update(void);
void Jump_To_App(void);
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// 사용하는 UART 핸들러 (CubeMX에서 printf용으로 설정한 UART, 보통 huart2)
extern UART_HandleTypeDef huart2; 

// GCC 환경에서 printf를 UART로 연결
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init(); // 
      // MX_USART6_UART_Init(); // 디버그용 UART
    HAL_Delay(2000);
    for(int i=0; i<3; i++) {
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); // 보드 LED 핀에 맞게 수정
          HAL_Delay(100);
          HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
          HAL_Delay(100);
    }
    
    printf("\r\n=================================\r\n");
    printf("[Bootloader] System Started!\r\n");
    printf("=================================\r\n");

    // 부트로더 동작 확인용 LED 점등
    // 동작 확인용: 부트로더 진입 시 LED 빠르게 3번 점멸
    
    printf("\r\n--- STM32 Bootloader Started ---\r\n");

    // 업데이트 플래그 확인
    uint32_t ota_flag = *(__IO uint32_t*)OTA_FLAG_ADDRESS;
    if (ota_flag == OTA_UPDATE_MAGIC_NUM) {
        Perform_OTA_Update(); // 업데이트 수행
    } else {
        printf("[BOOT] Normal Boot.\r\n");
    }

    
    // 메인 앱으로 이동
    Jump_To_App();

    while (1) {
        // Jump에 실패하면 여기 갇힘 (LED 무한 깜빡임 처리)
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(500);
    }
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
 void Perform_OTA_Update(void) {
    printf("[BOOT] OTA Update Detected! Starting copy...\r\n");
    
    HAL_FLASH_Unlock();
    
    // 1. 기존 앱 영역(Sector 2,3,4,5) 지우기
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_2;
    EraseInitStruct.NbSectors = 4; // Sector 2, 3, 4, 5
    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

    // 2. 다운로드 영역(Sector 6)에서 앱 영역으로 복사 (4바이트씩)
    uint32_t *pSource = (uint32_t *)DOWNLOAD_PARTITION_ADDR;
    uint32_t destAddr = APPLICATION_ADDRESS;
    
    // 다운로드된 파일의 끝을 알 수 없으므로, 여유 있게 넉넉히 복사하거나 
    // 파일 크기 정보를 따로 저장해야 합니다. 여기서는 최대 사이즈 복사 가정.
    for (uint32_t i = 0; i < APP_MAX_SIZE; i += 4) {
        if (*pSource != 0xFFFFFFFF) { // 빈 데이터가 아니면 기록
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destAddr, *pSource);
        }
        pSource++;
        destAddr += 4;
    }

    // 3. 업데이트 완료 후 Sector 1(플래그) 지우기
    EraseInitStruct.Sector = FLASH_SECTOR_1;
    EraseInitStruct.NbSectors = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
    
    HAL_FLASH_Lock();
    printf("[BOOT] OTA Update Complete!\r\n");
}



void Jump_To_App(void) {
    printf("[BOOT] Jumping to Gimbal App...\r\n");
    
    // 애플리케이션 주소에 유효한 스택 포인터가 있는지 확인
    if (((*(__IO uint32_t*)APPLICATION_ADDRESS) & 0x2FFE0000 ) == 0x20000000) {
        // 모든 주변장치 초기화 해제 (매우 중요! 안 하면 앱에서 충돌남)
        HAL_UART_DeInit(&huart2);
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        uint32_t JumpAddress = *(__IO uint32_t*) (APPLICATION_ADDRESS + 4);
        pFunction Jump = (pFunction) JumpAddress;

        // 메인 앱의 스택 포인터 설정
        __set_MSP(*(__IO uint32_t*) APPLICATION_ADDRESS);
        
        Jump(); // 짐벌 앱으로 점프!
    } else {
        printf("[BOOT] Error: No valid app found!\r\n");
    }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM10 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM10)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
