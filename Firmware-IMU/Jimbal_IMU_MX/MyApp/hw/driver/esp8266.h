#ifndef HW_DRIVER_ESP8266_H_
#define HW_DRIVER_ESP8266_H_

#include "hw_def.h" // HAL 라이브러리 및 시스템 정의가 포함된 헤더
#include "def.h"
#include "cli.h"
// ESP8266 초기화 (Wi-Fi 연결 등)
bool esp8266_Init(void);

// S3에서 펌웨어를 다운로드하여 지정된 Flash 주소에 기록
bool esp8266_DownloadOTA(const char* url, const char* filepath, uint32_t flash_addr);

#endif /* HW_DRIVER_ESP8266_H_ */