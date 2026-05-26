#include "stm32f4xx_hal.h"

#define DOWNLOAD_PARTITION_ADDR 0x08040000 // Sector 6 Start Address

void WriteFirmwareChunkToFlash(uint32_t flash_address, uint8_t *data, uint16_t length) {
    HAL_FLASH_Unlock();
    
    // Note: You must erase Sector 6 & 7 before writing the first chunk!
    
    for (uint16_t i = 0; i < length; i += 4) {
        // Pack 4 bytes into a 32-bit word
        uint32_t word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_address + i, word);
    }
    
    HAL_FLASH_Lock();
}

// When download finishes, trigger the bootloader
void TriggerReboot() {
    NVIC_SystemReset(); 
}