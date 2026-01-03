#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>

void array_cpy(void *source, void *receiver, int len);

uint8_t RangeCheck(uint8_t val, uint8_t min, uint8_t max);

uint8_t GetUInt8(uint8_t *buff, int16_t offset);
uint16_t GetUInt16(uint8_t *buff, int16_t offset);
uint32_t GetUInt32(uint8_t *buff, int16_t offset);

void SetBitUInt8(uint8_t *bits, uint8_t num);
void ResBitUInt8(uint8_t *bits, uint8_t num);

void UInt32ToBuff(uint8_t *buff, uint32_t val);
void UInt16ToBuff(uint8_t *buff, uint16_t val);
void UInt16ToBuffBigEndian(uint8_t *buff, uint16_t val);
void Int16ToBuff(uint8_t *buff, int16_t val);
#endif