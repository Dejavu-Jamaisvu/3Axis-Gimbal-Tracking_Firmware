#include "ap.h"


// FreeRTOS 세마포어 핸들
extern osSemaphoreId_t GyroReadySemHandle;


static volatile uint32_t led_toggle_period = 0;
static volatile uint32_t imu_report_period = 0;
static volatile uint32_t gimbal_report_period = 0;

static volatile bool imu_report_enabled = false; 
// static bool is_gyro_ok = false;
static bool is_mag_ok = false;



void cliServo(uint8_t argc, char **argv) {
    if (argc >= 2) {
        
        if (strcmp(argv[1], "total") == 0) {
            servoTotalTest();
            return; // 성공 시 즉시 종료
        }

        if (strcmp(argv[1], "test") == 0 && argc == 4) {
            uint8_t ch = (uint8_t)atoi(argv[2]);
            uint8_t angle = (uint8_t)atoi(argv[3]);
            if (ch <= 2) {
                servoWrite(ch, (angle > 180) ? 180 : angle);
                cliPrintf("Servo[%d] set to %d deg\r\n", ch, angle);
                return; // 성공 시 즉시 종료
            }
        }

        if (strcmp(argv[1], "scan") == 0 && argc == 3) {
            servoScan((uint8_t)atoi(argv[2]));
            return;
        }

        // if (strcmp(argv[1], "smooth") == 0 && argc == 5) {
        //     uint8_t ch = (uint8_t)atoi(argv[2]);
        //     float angle = (float)atof(argv[3]);
        //     float k = (float)atof(argv[4]);
        //     servoSetTarget(ch, angle, k);
        //     cliPrintf("Servo %d: Target %.1f, k %.2f\r\n", ch, angle, k);
        //     return;
        // }

        // if (strcmp(argv[1], "sync") == 0 && argc == 6) {
        //     float a0 = (float)atof(argv[2]);
        //     float a1 = (float)atof(argv[3]);
        //     float a2 = (float)atof(argv[4]);
        //     float k  = (float)atof(argv[5]);
        //     servoSetTargetAll(a0, a1, a2, k);
        //     cliPrintf("Sync Target Set\r\n");
        //     return;
        // }

        // if (strcmp(argv[1], "sweep") == 0 && argc == 4) {
        //     uint8_t ch = (uint8_t)atoi(argv[2]);
        //     if (ch > 2) {
        //         cliPrintf("Error: Invalid Channel %d\r\n", ch);
        //         return; // 잘못된 채널이면 Usage를 보여주는 대신 에러 메시지 후 종료하는 게 명확할 때가 많습니다.
        //     }
        //     float k = (float)atof(argv[3]);
        //     servoSweep(ch, k);
        //     cliPrintf("CH %d Sweep\r\n", ch);
        //     return;
        // }

        if (strcmp(argv[1], "smooth") == 0 && argc == 5) {
            // 1. 셋팅 (이미 있는 함수 활용)
            servoSetTarget((uint8_t)atoi(argv[2]), (float)atof(argv[3]), (float)atof(argv[4]));
            
            // 2. 셋팅된 대로 돌리기
            servoRunToTarget((uint8_t)atoi(argv[2]));
            return;
        }

        if (strcmp(argv[1], "sync") == 0 && argc == 6) {
            // 1. 셋팅
            servoSetTargetAll((float)atof(argv[2]), (float)atof(argv[3]), (float)atof(argv[4]), (float)atof(argv[5]));
            
            // 2. 셋팅된 대로 돌리기 (3개 채널)
            for(int i=0; i<3; i++) {
                servoRunToTarget(i); 
            }
            return;
        }

        if (strcmp(argv[1], "sweep") == 0 && argc == 4) {
            // 1. 셋팅 (servoSweep이 알아서 반대편 타겟을 셋팅함)
            servoSweep((uint8_t)atoi(argv[2]), (float)atof(argv[3]));
            
            // 2. 셋팅된 대로 돌리기
            servoRunToTarget((uint8_t)atoi(argv[2]));
            return;
        }
    }

    // 3. 인자가 부족했거나(argc < 2), 위 if문 조건(명령어 오타, 인자 개수 틀림)에 하나도 안 걸렸다는 뜻입니다.
    cliPrintf("\r\n[Invalid Command or Arguments]\r\n");
    cliPrintf("Usage:\r\n");
    cliPrintf("  servo total\r\n");
    cliPrintf("  servo test [0-2] [0-180]\r\n");
    cliPrintf("  servo scan [0-2]\r\n");
    cliPrintf("  servo smooth [0-2] [0-180] [k]\r\n");
    cliPrintf("  servo sync [ang0] [ang1] [ang2] [k]\r\n");
    cliPrintf("  servo sweep [0-2] [k]\r\n");
}



// button on/off  => enable/disable
void cliButton(uint8_t argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "on") == 0) {
            buttonEnable(true);
            cliPrintf("Button Interrupt Report: ON\r\n");
        } else if (strcmp(argv[1], "off") == 0) {
            buttonEnable(false);
            cliPrintf("Button Interrupt Report: OFF\r\n");
        }
    } else {
        cliPrintf("Usage: button [on/off]\r\n");
        cliPrintf("Current Status: %s\r\n", buttonGetEnable() ? "ON" : "OFF");
    }
}




// ==========================================
// CLI Commands
// ==========================================
void cliImu(uint8_t argc, char **argv) {
  if (argc >= 2) {
    if (strcmp(argv[1], "on") == 0) {
      uint32_t period = (argc == 3) ? atoi(argv[2]) : 500; 
      if (period < 100) period = 100;
      
      // 하드웨어 재초기화 및 주기 설정 위임
      if(!gyroServiceReInit()) {
          cliPrintf("ERROR: Sensor Init Failed. Check Wires.\r\n");
          return;
      }

      // 🌟 2. 센서 안정화 대기 (DLPF 및 상보필터가 자리를 잡을 시간)
      cliPrintf("Waiting for IMU stabilization...\r\n");
      osDelay(1000); // 1초 정도 대기하면 폭주 값이 사라집니다.
    
    
      gyroServiceSyncCurrentOrigin();
      gyroServiceSetPeriod(period);

      
      cliPrintf("IMU Report : ON (%d ms)\r\n", period);

    } else if (strcmp(argv[1], "off") == 0) {
      gyroServiceSetPeriod(0);
      cliPrintf("IMU Report : OFF\r\n");
    }
  } else {
      // 💡 가이드 메시지 추가 (정리 포인트)
      cliPrintf("Usage: imu [on|off] [period]\r\n");
  }
}


static bool isSafeAddress(uint32_t addr)
{
  // 1. f411 flash
  if (0x08000000 <= addr && addr <= 0x0807FFFF)
    return true;

  // 2. f411 ram
  if (0x20000000 <= addr && addr <= 0x20001FFF)
    return true;

  // 3. system memory
  if (0x1FFF0000 <= addr && addr <= 0x1FFF7A1F)
    return true;

  // 4. Peripheral register
  if (0x40000000 <= addr && addr <= 0x5FFFFFFF)
    return true;

  return false;
}

// md 0x8000-0000 32
void cliMd(uint8_t argc, char **argv)
{
  if (argc >= 2)
  {
    uint32_t addr = strtoul(argv[1], NULL, 16);
    uint32_t length = 16;

    if (argc >= 3)
    {
      length = strtoul(argv[2], NULL, 0);
    }

    for (uint32_t i = 0; i < length; i += 16)
    {
      cliPrintf("0x%08x : ", addr + i);

      for (uint32_t j = 0; j < 16; j++)
      {
        if (i + j < length)
        {
          uint32_t target_addr = (addr + i + j);
          if (isSafeAddress(target_addr))
          {
            uint8_t val = *((volatile uint8_t *)target_addr);
            cliPrintf("%02X ", val);
          }
          else
          {
            cliPrintf("Not valid address!!\r\n");
            break;
          }
        }
        else
        {
          cliPrintf("   ");
        }
      }
      //
      cliPrintf(" | ");

      for (uint32_t j = 0; j < 16; j++)
      {
        if (i + j < length)
        {
          uint32_t target_addr = (addr + i + j);
          if (isSafeAddress(target_addr))
          {
            uint8_t val = *((volatile uint8_t *)target_addr);
            if (val >= 0x20 && val <= 0x7E)
            {
              cliPrintf("%c", val);
            }
            else
            {
              cliPrintf(".");
            }
          }
          else
          {
            cliPrintf("Not valid address!!\r\n");
            break;
          }
        }
      }

      cliPrintf("\r\n");
    }
  }
  else
  {
    cliPrintf("Usage : md [add(hex)] [length]\r\n");
    cliPrintf("        md 08000000 32 \r\n");
  }
}


#include "ap.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

#define OTA_FLAG_ADDRESS        0x08004000 // Sector 1 시작 주소
#define DOWNLOAD_PARTITION_ADDR 0x08040000 // Sector 6 시작 주소
#define OTA_UPDATE_MAGIC_NUM    0xDEADBEEF // 업데이트 대기 플래그

// ESP8266을 통해 다운로드하고 플래시에 쓰는 가상의 함수 (앞선 답변의 로직 활용)
extern bool ESP8266_DownloadToFlash(const char* url, uint32_t flash_addr);
// CLI 처리 함수
void cliOTA(uint8_t argc, char **argv) {
    // "ota on" 명령어 입력 시
    if (argc > 1 && strcmp(argv[1], "on") == 0) {
        printf("\r\n[CLI] Starting OTA Update Process...\r\n");

        // 1. ESP8266 초기화
        esp8266_Init();

        // 2. 펌웨어 다운로드 및 Sector 6에 저장
        const char* s3_host = "gimbal-s3-imu-elf-file.s3.us-east-1.amazonaws.com";
        const char* s3_file = "/Jimbal_IMU_MX.bin";
        
        bool success = esp8266_DownloadOTA(s3_host, s3_file, DOWNLOAD_PARTITION_ADDR);

        if (success) {
            printf("[CLI] OTA Ready. Setting Boot Flag...\r\n");
            
            // 3. Sector 1에 OTA 업데이트 대기 플래그 기록
            HAL_FLASH_Unlock();
            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.Sector = FLASH_SECTOR_1;
            EraseInitStruct.NbSectors = 1;
            HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
            
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, OTA_FLAG_ADDRESS, OTA_UPDATE_MAGIC_NUM);
            HAL_FLASH_Lock();

            // 4. 시스템 재부팅 (자동으로 부트로더로 진입)
            printf("[CLI] Rebooting to Bootloader in 2 seconds...\r\n");
            HAL_Delay(2000);
            NVIC_SystemReset(); 
        } else {
            printf("[CLI] OTA Download Failed. Aborting.\r\n");
        }
    } 
    else {
        printf("Usage: ota on\r\n");
    }
}



// argv[1] : "read" "write"
// argv[2] : pin "A5", "B12"
void cliGpio(uint8_t argc, char **argv)
{
  if (argc >= 3)
  {
    char port_char = tolower(argv[2][0]);
    int pin_num = atoi(&argv[2][1]);

    uint8_t port_idx = port_char - 'a';

    if (strcmp(argv[1], "read") == 0)
    {
      int8_t state = gpioExtRead(port_idx, pin_num);
      if (state < 0)
      {
        cliPrintf("Invalid Port or Pin (ex:a5, b12)\r\n");
      }
      else
      {
        cliPrintf("GPIO %c%d=%d\r\n", toupper(port_char), pin_num, state);
      }
    }
    else if (strcmp(argv[1], "write") == 0 && argc == 4)
    {
      int val = atoi(argv[3]);
      if (gpioExtWrite(port_idx, pin_num, val) == true)
      {
        cliPrintf("GPIO %c%d Set to %d\r\n", toupper(port_char), pin_num, val);
      }
      else
      {
        cliPrintf("Invalid Port or Pin (ex:a5, b12)\r\n");
      }
    }
    else
    {
      cliPrintf("Usage: gpio read [a~h][0~15]\r\n");
      cliPrintf("       gpio write [a~h][0~15] [0|1]\r\n");
    }
  }
  else
  {
    cliPrintf("Usage: gpio read [a~h][0~15]\r\n");
    cliPrintf("       gpio write [a~h][0~15] [0|1]\r\n");
  }
}

// static uint32_t led_toggle_period = 0;
void cliLed(uint8_t argc, char **argv)
{
  if (argc >= 2)
  {
    if (strcmp(argv[1], "on") == 0)
    {
      led_toggle_period=0;
      ledOn();
      LOG_INF("LED ON");
    }
    else if (strcmp(argv[1], "off") == 0)
    {
      led_toggle_period=0;
      ledOff();
      LOG_INF("LED OFF");
    }
    else if (strcmp(argv[1], "toggle") == 0)
    {
      if(argc==3){
        led_toggle_period=atoi(argv[2]);
        if(led_toggle_period>0){
          LOG_INF("LED  Auto-Toggled!!");
        }
        else{
          cliPrintf("Invalid Period\r\n");
        }

      }
      else{
        led_toggle_period=0;
        ledToggle();
        LOG_INF("LED TOGGLE");
      }
    }
    else
    {
      cliPrintf("Invalid Command\r\n");
    }
  }
  else
  {
    cliPrintf("Usage: led [on|off]\r\n");
    cliPrintf("     : led toggle\r\n");
    cliPrintf("     : led toggle [period]\r\n");
  }
}

void cliInfo(uint8_t argc, char **argv)
{
  if (argc == 1)
  {
    cliPrintf("===============================");
    cliPrintf("  HW Model   :  STM32F411\r\n");
    cliPrintf("  FW Version : V1.0.0\r\n");
    cliPrintf("  Build Date : %s %s\r\n", __DATE__, __TIME__);
    uint32_t uid0 = HAL_GetUIDw0();
    uint32_t uid1 = HAL_GetUIDw1();
    uint32_t uid2 = HAL_GetUIDw2();
    uint32_t dev = HAL_GetDEVID();

    cliPrintf("  Serial Num : %08x-%08x-%08x\r\n", uid0, uid1, uid2);
    cliPrintf("  DevicID    : %08x\r\n", dev);

    cliPrintf("===============================\r\n");
  }
  else if (argc == 2 || strcmp(argv[1], "uptime") == 0)
  {
    cliPrintf("System Uptime: %d ms \r\n", millis());
  }
  else
  {
    cliPrintf("Usage: info\r\n");
    cliPrintf("       info [uptime]\r\n");
  }
}
void cliSys(uint8_t argc, char **argv)
{
  if ((argc == 2) && strcmp(argv[1], "reset") == 0)
  {
    NVIC_SystemReset();
  }
  else
  {
    cliPrintf("Usage: sys [reset]\r\n");
  }
}




// ==========================================
// Gimbal CLI Command
// ==========================================
void cliGimbal(uint8_t argc, char **argv) {
  if (argc >= 2) {
    if (strcmp(argv[1], "on") == 0) {
      uint32_t period = (argc == 3) ? atoi(argv[2]) : 500; // 기본 500ms
      
      // UART 병목 방지 (텍스트 모드 시 최소 100ms 제한)
      if(!isMonitoringOn() && period < 100) {
          period = 100;
          cliPrintf("Warning: Period limited to 100ms to prevent UART bottleneck.\r\n");
      }
      
      gimbal_report_period = period;
      cliPrintf("Gimbal Report : ON (%d ms)\r\n", gimbal_report_period);
    } 
    else if (strcmp(argv[1], "off") == 0) {
      gimbal_report_period = 0;
      gimbalReturnHome();
      cliPrintf("Gimbal Report : OFF & return Home\r\n");
    }// 2. gimbal cam on/off 처리 (strcmp 반환값 및 argc 체크 수정)
    else if (strcmp(argv[1], "cam") == 0) { // == 0 추가
        if (argc >= 3) { // argv[2]를 쓰려면 argc가 최소 3이어야 함
            if (strcmp(argv[2], "on") == 0) { // == 0 추가
                cameraServiceResetZeroPoint();  
                gyroServiceSyncCurrentOrigin();
                gimbalSetCamControlEnable(true);
                // gimbalReturnHome(); 
                cliPrintf("Gimbal Camera Control: ON\r\n");
            } 
            else if (strcmp(argv[2], "off") == 0) { // == 0 추가
                gimbalSetCamControlEnable(false);
                cliPrintf("Gimbal Camera Control: OFF\r\n");
            }
            else {
                cliPrintf("Usage: gim cam [on|off]\r\n");
            }
        } else {
            cliPrintf("Usage: gim cam [on|off]\r\n");
        }
    }
  } else {
    cliPrintf("Usage: gim [on|off] [period]\r\n");
    cliPrintf("       gim cam [on|off]\r\n");
  }
}

void gimbalSystemTask(void *argument)
{
    LOG_INF("Gimbal System Task Started!");


    cameraServiceInit();   // 카메라 PID 변수(sum, prev) 초기화

    while (1) {


        // 카메라 업데이트 (파싱 + PID 계산 + 보고)
        bool has_new_cam_data = cameraDataParsing(); // 이제 에러 안 나고 true/false를 받음!

        if (has_new_cam_data) {
            cameraServicePIDUpdate(); // 새 데이터가 왔을 때만 PID 계산!
        }
        // 통합 제어 (데이터 합산 및 서보 구동)
        gimbalExecuteCombinedControl();

        osDelay(10); // 100Hz
    }
}




void StartDefaultTask(void *argument)
{
  apInit();
  for(;;)
  {
    apMain();
  }
  
}



void ledSystemTask(void *argument)
{
  while (1)
  {
    if(led_toggle_period > 0){
      
      ledToggle();
      bool led_state=ledGetStatus();

      if(isMonitoringOn()){
        monitorUpdateValue(ID_OUT_LED_STATE,TYPE_BOOL,&led_state);
      }
      else{
        LOG_DBG("LED Toggle!");
      }
      
      osDelay(led_toggle_period);
    }
    else{
      bool led_state=ledGetStatus();
      if(isMonitoringOn()){
        monitorUpdateValue(ID_OUT_LED_STATE,TYPE_BOOL,&led_state);
      }
      osDelay(50);
    }
  }
}


static uint32_t monitor_period = 0;
void monitorSystemTask(void *argument){
  while(1){
    if(isMonitoringOn()){
      monitorSendPacket();
    }
    monitor_period=monitorGetPeriod();
    osDelay(monitor_period);
  }

}

void gyroSystemTask(void *argument) {
  while (1) {
    // 변수 직접 접근 대신 Getter 함수 사용
    if (gyroServiceIsOk() && GyroReadySemHandle != NULL) { 
        gyroServiceUpdate();
    } else {
        osDelay(10);
    }
  }
}


void magSystemTask(void *argument) {
    Mag_Data_t magData; // 지역 변수로 선언

    while(1) {
        if (is_mag_ok) {
            if (Mag_Read(&magData)) {
                // 읽어온 데이터를 gyro_service로 전달
                gyroServiceSetMagData(&magData);
            }
        }
        osDelay(10); // 지자기 센서는 자이로보다 느리게 갱신해도 무방합니다
    }
}

// extern osThreadId_t defaultTaskHandle;
// extern osThreadId_t myTaskLedHandle;;
// extern osThreadId_t myTaskGyroHandle;
// extern osThreadId_t myTaskMonitorHandle;
// extern osThreadId_t myTaskMagHandle;
// extern osThreadId_t myTaskGimbalHandle;
// extern osThreadId_t stackMonitorTasHandle;

// static uint32_t task1Stack, task2Stack, task3Stack, task4Stack, task5Stack, task6Stack, task7Stack;
// void showStack() {
//   task1Stack = osThreadGetStackSpace(defaultTaskHandle);
//   task2Stack = osThreadGetStackSpace(myTaskLedHandle);
//   task3Stack = osThreadGetStackSpace(myTaskGyroHandle);
//   task4Stack = osThreadGetStackSpace(myTaskMonitorHandle);
//   task5Stack = osThreadGetStackSpace(myTaskMagHandle);
//   task6Stack = osThreadGetStackSpace(myTaskGimbalHandle);
//   task7Stack = osThreadGetStackSpace(stackMonitorTasHandle);

//   printf("Stack free - Task1: %lu Byte\t Task2: %lu Byte\t Task3: %lu Byte\t \r\n", task1Stack, task2Stack, task3Stack);
//   printf("Task4: %lu Byte\t Task5: %lu Byte\t Task6: %lu Byte\t Task7: %lu Byte\r\n", task4Stack, task5Stack, task6Stack, task7Stack);
// }

// void startMonitorStartTask(void *argument){
//   for(;;){
//     osDelay(5000);
//     showStack();
//   }
// }

void apStopAutoTask(void){
  monitorOff();
  led_toggle_period = 0;
  imu_report_period = 0;
  gimbal_report_period = 0;
  Gyro_StopAuto();
  Mag_StopAuto();
  gimbalReturnHome();
  ledOff();
}

void apSyncPeriods(uint32_t period){
  if(period > 0) {
    led_toggle_period = period;
    imu_report_period = period;
    Gyro_StartAuto();
    Mag_StartAuto();
    LOG_INF("Task Synchronized to %d ms", period);
  } else {
    led_toggle_period = 0;
    imu_report_period = 0;
    Gyro_StopAuto();
    Mag_StopAuto();
  }
}

void apInit(void)
{   
  hwInit(); 
  LOG_INF("Application Init... Started");
  logInit();
  monitorInit();

  
  servoInit();
  // 모터가 90도까지 '물리적으로' 이동할 시간
  // osDelay(1500);


  monitorSetSyncHandler(apSyncPeriods);
  cliSetCtrlCHandler(apStopAutoTask);
  
// ==========================================
  // [강력 추천] I2C Scanner 디버깅 코드
  // ==========================================
  extern I2C_HandleTypeDef hi2c1;
  cliPrintf("-------------------------------\r\n");
  cliPrintf("I2C Bus Scanner Started...\r\n");
  int found_count = 0;
  for(uint16_t i = 1; i < 128; i++) {
      if(HAL_I2C_IsDeviceReady(&hi2c1, (i << 1), 3, 100) == HAL_OK) {
          cliPrintf(">>> Found I2C Device at 0x%02X\r\n", i);
          found_count++;
      }
  }
  if (found_count == 0) cliPrintf(">>> NO I2C DEVICES FOUND! Check Wiring.\r\n");
  cliPrintf("-------------------------------\r\n");

  // ==========================================
  // 서비스 및 센서 초기화 (Encapsulated)
  // ==========================================
  
  // 자이로 서비스 초기화 (내부에서 Gyro_Init 호출 및 상태 저장)
  if(!gyroServiceInit()) {
      LOG_ERR("MPU6050 Init Failed.");
  } else {
      LOG_INF("MPU6050 Init OK.");
  }
  
  // 지자기 센서 초기화 (필요시 magService로 분리 가능)
  is_mag_ok = Mag_Init(); 
  if(!is_mag_ok) LOG_ERR("HMC5883L Init Failed.");
  else LOG_INF("HMC5883L Init OK.");



  cliAdd("led", cliLed);
  cliAdd("info", cliInfo);
  cliAdd("sys", cliSys);
  cliAdd("gpio", cliGpio);
  cliAdd("md", cliMd);
    cliAdd("button", cliButton);
    // cliAdd("temp", cliTemp);    
  cliAdd("imu", cliImu); // IMU 명령어 추가
  cliAdd("servo", cliServo);
  cliAdd("gim", cliGimbal);
  cliAdd("ota", cliOTA);


}
void apMain(void)
{
  
  uartPrintf(0, "LED Task Started!!\r\n");
  while (1)
  {
    cliMain();
    osDelay(1);
  }
}


// void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
// {
//    /* Run time stack overflow checking is performed if
//    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
//    called if a stack overflow is detected. */
//   HAL_UART_DeInit(&huart3);
//   HAL_UART_Init(&huart3);

//   char msg[100];
//   int len = snprintf(msg, sizeof(msg), "Stack Overflow detectedin Task: %s\r\n", pcTaskName);
//   HAL_UART_Transmit(&huart3, (uint8_t*)msg, len, HAL_MAX_DELAY);
//   showStack();

//   while(1);
   
// }