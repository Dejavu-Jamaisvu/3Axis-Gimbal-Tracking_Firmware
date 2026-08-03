/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for myTaskLed */
osThreadId_t myTaskLedHandle;
const osThreadAttr_t myTaskLed_attributes = {
  .name = "myTaskLed",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for myTaskGyro */
osThreadId_t myTaskGyroHandle;
const osThreadAttr_t myTaskGyro_attributes = {
  .name = "myTaskGyro",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal3,
};
/* Definitions for myTaskMonitor */
osThreadId_t myTaskMonitorHandle;
const osThreadAttr_t myTaskMonitor_attributes = {
  .name = "myTaskMonitor",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal5,
};
/* Definitions for myTaskMag */
osThreadId_t myTaskMagHandle;
const osThreadAttr_t myTaskMag_attributes = {
  .name = "myTaskMag",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal3,
};
/* Definitions for myTaskGimbal */
osThreadId_t myTaskGimbalHandle;
const osThreadAttr_t myTaskGimbal_attributes = {
  .name = "myTaskGimbal",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal3,
};
/* Definitions for GyroReadySem */
osSemaphoreId_t GyroReadySemHandle;
const osSemaphoreAttr_t GyroReadySem_attributes = {
  .name = "GyroReadySem"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void ledSystemTask(void *argument);
void gyroSystemTask(void *argument);
void monitorSystemTask(void *argument);
void magSystemTask(void *argument);
void gimbalSystemTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of GyroReadySem */
  GyroReadySemHandle = osSemaphoreNew(1, 1, &GyroReadySem_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of myTaskLed */
  myTaskLedHandle = osThreadNew(ledSystemTask, NULL, &myTaskLed_attributes);

  /* creation of myTaskGyro */
  myTaskGyroHandle = osThreadNew(gyroSystemTask, NULL, &myTaskGyro_attributes);

  /* creation of myTaskMonitor */
  myTaskMonitorHandle = osThreadNew(monitorSystemTask, NULL, &myTaskMonitor_attributes);

  /* creation of myTaskMag */
  myTaskMagHandle = osThreadNew(magSystemTask, NULL, &myTaskMag_attributes);

  /* creation of myTaskGimbal */
  myTaskGimbalHandle = osThreadNew(gimbalSystemTask, NULL, &myTaskGimbal_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
__weak void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_ledSystemTask */
/**
* @brief Function implementing the myTaskLed thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ledSystemTask */
__weak void ledSystemTask(void *argument)
{
  /* USER CODE BEGIN ledSystemTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END ledSystemTask */
}

/* USER CODE BEGIN Header_gyroSystemTask */
/**
* @brief Function implementing the myTaskGyro thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_gyroSystemTask */
__weak void gyroSystemTask(void *argument)
{
  /* USER CODE BEGIN gyroSystemTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END gyroSystemTask */
}

/* USER CODE BEGIN Header_monitorSystemTask */
/**
* @brief Function implementing the myTaskMonitor thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_monitorSystemTask */
__weak void monitorSystemTask(void *argument)
{
  /* USER CODE BEGIN monitorSystemTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END monitorSystemTask */
}

/* USER CODE BEGIN Header_magSystemTask */
/**
* @brief Function implementing the myTaskMag thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_magSystemTask */
__weak void magSystemTask(void *argument)
{
  /* USER CODE BEGIN magSystemTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END magSystemTask */
}

/* USER CODE BEGIN Header_gimbalSystemTask */
/**
* @brief Function implementing the myTaskGimbal thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_gimbalSystemTask */
__weak void gimbalSystemTask(void *argument)
{
  /* USER CODE BEGIN gimbalSystemTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END gimbalSystemTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

