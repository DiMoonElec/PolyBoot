#ifndef __CRC16_H__
#define __CRC16_H__

#include <stdint.h>

void Crc16Init(void);
uint16_t Crc16StartValue(void);
uint16_t Crc16(uint8_t *pcBlock, uint16_t len, uint16_t start_crc);

#endif
