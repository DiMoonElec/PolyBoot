#include <stdint.h>
#include "utils.h"

void array_cpy(void *source, void *receiver, int len)
{
  for (int i = 0; i < len; i++)
  {
    ((uint8_t *)receiver)[i] = ((uint8_t *)source)[i];
  }
}

uint8_t RangeCheck(uint8_t val, uint8_t min, uint8_t max)
{
  if ((val < min) && (val > max))
    return 1;
  return 0;
}

uint8_t GetUInt8(uint8_t *buff, int16_t offset)
{
  return buff[offset];
}

uint16_t GetUInt16(uint8_t *buff, int16_t offset)
{
  uint16_t val = buff[offset] | (buff[offset + 1] << 8);
  return val;
}

uint32_t GetUInt32(uint8_t *buff, int16_t offset)
{
  uint32_t val = (buff[offset]) | (buff[offset + 1] << 8) | (buff[offset + 2] << 16) | (buff[offset + 3] << 24);
  return val;
}

void SetBitUInt8(uint8_t *bits, uint8_t num)
{
  (*bits) |= (1 << num);
}

void ResBitUInt8(uint8_t *bits, uint8_t num)
{
  (*bits) &= ~(1 << num);
}

void UInt32ToBuff(uint8_t *buff, uint32_t val)
{
  buff[0] = (uint8_t)(val);
  buff[1] = (uint8_t)(val >> 8);
  buff[2] = (uint8_t)(val >> 16);
  buff[3] = (uint8_t)(val >> 24);
}

void UInt16ToBuff(uint8_t *buff, uint16_t val)
{
  buff[0] = (uint8_t)(val);
  buff[1] = (uint8_t)(val >> 8);
}

void UInt16ToBuffBigEndian(uint8_t *buff, uint16_t val)
{
  buff[0] = (uint8_t)(val >> 8);
  buff[1] = (uint8_t)(val);
}

void Int16ToBuff(uint8_t *buff, int16_t val)
{
  buff[0] = (uint8_t)(val);
  buff[1] = (uint8_t)(val >> 8);
}