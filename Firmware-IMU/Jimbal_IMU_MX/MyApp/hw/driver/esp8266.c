#include "esp8266.h"
#include "cli.h"


extern UART_HandleTypeDef huart6; // ESP8266과 연결된 UART 핸들
// extern UART_HandleTypeDef huart2; // 디버그 메시지 출력용 (printf가 huart2로 리다이렉트 되었다고 가정)

// AT 명령어 전송 헬퍼 함수
static void Send_AT_Cmd(const char* cmd) {
    HAL_UART_Transmit(&huart6, (uint8_t*)cmd, strlen(cmd), 1000);
    printf("TX: %s", cmd);
}

bool esp8266_Init(void) {
    printf("[ESP8266] Initializing...\r\n");
    cliPrintf("[ESP8266] Initializing...\r\n");
    Send_AT_Cmd("AT+CWMODE=1\r\n");
    HAL_Delay(1000);
    
    // 본인의 Wi-Fi 공유기 이름과 비밀번호로 변경하세요.
    Send_AT_Cmd("AT+CWJAP=\"Your_SSID\",\"Your_PASSWORD\"\r\n");
    HAL_Delay(5000); // 연결될 때까지 충분한 대기 시간 필요
    
    Send_AT_Cmd("AT+CIPSSLSIZE=4096\r\n");
    HAL_Delay(500);
    
    return true; // 단순화를 위해 무조건 true 반환. 실무에서는 "OK" 응답을 파싱해야 함.
}

bool esp8266_DownloadOTA(const char* host, const char* filepath, uint32_t flash_addr) {
    char cmd_buffer[256];
    
    printf("[ESP8266] Connecting to AWS S3...\r\n");
    
    // 1. SSL 연결 시작
    sprintf(cmd_buffer, "AT+CIPSTART=\"SSL\",\"%s\",443\r\n", host);
    Send_AT_Cmd(cmd_buffer);
    HAL_Delay(3000); // 서버 연결 대기

    // 2. HTTP GET 요청 헤더 만들기
    char get_req[512];
    sprintf(get_req, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", filepath, host);
    cliPrintf("GET Request: %s\r\n", get_req);          
    sprintf(cmd_buffer, "AT+CIPSEND=%d\r\n", strlen(get_req));
    Send_AT_Cmd(cmd_buffer);
    cliPrintf("SEND AT CMD: %s\r\n", cmd_buffer);
    HAL_Delay(500);
    
    // 3. HTTP GET 요청 전송
    Send_AT_Cmd(get_req);
    printf("[ESP8266] Request sent. Downloading...\r\n");
    cliPrintf("[ESP8266] Request sent. Downloading...\r\n: %s\r\n", get_req);
    // =========================================================================
    // [중요 로직] 플래시 기록 부분
    // 실제로는 응답 문자열에서 "HTTP/1.1 200 OK"를 확인하고, 
    // "\r\n\r\n" 문자열 이후부터 나오는 순수 바이너리 데이터를 플래시에 적어야 합니다.
    // =========================================================================
    
    HAL_FLASH_Unlock();
    
    // Sector 6 지우기
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_6;
    EraseInitStruct.NbSectors = 1;
    if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        printf("[ESP8266] Flash Erase Failed!\r\n");
        cliPrintf("[ESP8266] Flash Erase Failed!\r\n");
        HAL_FLASH_Lock();
        return false;
    }

    // [개념적 구현] 데이터 수신 및 기록
    // 주의: AT 통신으로 대용량 바이너리를 받을 때는 HAL_UART_Receive (폴링) 방식으로는
    // 바이트 유실(Data Loss)이 발생할 확률이 매우 높습니다. DMA와 링버퍼 구조가 필수적입니다.
    uint8_t rx_byte;
    uint32_t current_addr = flash_addr;
    uint8_t word_buffer[4];
    uint8_t word_idx = 0;
    uint32_t timeout = HAL_GetTick();

    // 임시 수신 루프 (데이터가 끊길 때까지 수신)
    while((HAL_GetTick() - timeout) < 5000) { // 5초간 데이터가 없으면 종료
        if(HAL_UART_Receive(&huart6, &rx_byte, 1, 10) == HAL_OK) {
            timeout = HAL_GetTick(); // 데이터가 들어오면 타임아웃 갱신
            
            // 여기서 실제로는 HTTP 헤더를 걸러내는 로직이 필요합니다.
            // 생략하고 순수 데이터라고 가정합니다.
            
            word_buffer[word_idx++] = rx_byte;
            
            // 4바이트가 모이면 플래시에 기록 (STM32F4는 플래시에 Word(32bit) 단위 기록 권장)
            if(word_idx == 4) {
                uint32_t word = word_buffer[0] | (word_buffer[1] << 8) | (word_buffer[2] << 16) | (word_buffer[3] << 24);
                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, word);
                current_addr += 4;
                word_idx = 0;
            }
        }
    }

    
    // 남은 자투리 바이트(1~3바이트)가 있다면 0xFF로 채워서 기록
    if(word_idx > 0) {
        while(word_idx < 4) word_buffer[word_idx++] = 0xFF;
        uint32_t word = word_buffer[0] | (word_buffer[1] << 8) | (word_buffer[2] << 16) | (word_buffer[3] << 24);
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, current_addr, word);
    }

    HAL_FLASH_Lock();
    printf("[ESP8266] Download & Flash Write Complete.\r\n");
    cliPrintf("[ESP8266] Download & Flash Write Complete.\r\n");
    return true;
}