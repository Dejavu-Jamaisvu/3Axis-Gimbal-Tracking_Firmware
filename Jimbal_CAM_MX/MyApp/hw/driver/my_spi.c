#include "my_spi.h"

void spiWriteByte(uint8_t data) {
    while (!(SPI1->SR & SPI_SR_TXE));       // 송신 버퍼 비었는지 확인
    *(volatile uint8_t *)&SPI1->DR = data;  // 8비트 전송
    while (SPI1->SR & SPI_SR_BSY);          // 전송 완료 대기
}

void spiWrite16(uint16_t data) {
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&SPI1->DR = (uint8_t)(data >> 8);  // 상위 바이트
    while (!(SPI1->SR & SPI_SR_TXE));
    *(volatile uint8_t *)&SPI1->DR = (uint8_t)(data & 0xFF); // 하위 바이트
    while (SPI1->SR & SPI_SR_BSY);
}