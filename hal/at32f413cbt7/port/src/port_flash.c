#include "at32f413.h"
#include "bootloader_port.h"
#include "bootloader_hal_config.h"
#include "bootloader_project_config.h"

#define FLASH_OP_FLAGS (FLASH_ODF_FLAG | FLASH_EPPERR_FLAG | FLASH_PRGMERR_FLAG)

uint8_t port_sector_isclear(uint32_t sector)
{
  for (uint32_t i = 0; i < BOOTLOADER_FLASH_SECTOR_SIZE; i += 4)
  {
    if (*(const volatile uint32_t *)(sector + i) != 0xFFFFFFFFUL)
      return 0;
  }
  return 1;
}

uint8_t port_sector_erase(uint32_t address)
{
  flash_status_type status;
  uint32_t primask;

  /* Стирание допускается только целиком внутри области приложения. */
  if ((address < BOOTLOADER_APP_BEGIN) ||
      (address > BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH - BOOTLOADER_FLASH_SECTOR_SIZE) ||
      ((address % BOOTLOADER_FLASH_SECTOR_SIZE) != 0))
    return 1;

  primask = __get_PRIMASK();
  __disable_irq();
  flash_unlock();
  flash_flag_clear(FLASH_OP_FLAGS);
  status = flash_sector_erase(address);
  flash_flag_clear(FLASH_OP_FLAGS);
  flash_lock();
  __set_PRIMASK(primask);

  return status != FLASH_OPERATE_DONE;
}

uint8_t port_write_chunk(uint8_t *chunk, uint32_t address, uint16_t len)
{
  flash_status_type status;
  uint32_t primask;
  uint32_t word;
  uint32_t count;

  if ((address < BOOTLOADER_APP_BEGIN) ||
      (address > BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH) ||
      (len > BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH - address))
    return 1;

  for (uint32_t i = 0; i < len; i += count)
  {
    count = 1;
    if ((((address + i) & 3U) == 0) && ((len - i) >= 4))
    {
      /* Буфер команды может быть не выровнен по границе слова. */
      word = (uint32_t)chunk[i] |
             ((uint32_t)chunk[i + 1] << 8) |
             ((uint32_t)chunk[i + 2] << 16) |
             ((uint32_t)chunk[i + 3] << 24);
      count = 4;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    flash_unlock();
    flash_flag_clear(FLASH_OP_FLAGS);
    if (count == 4)
      status = flash_word_program(address + i, word);
    else
      status = flash_byte_program(address + i, chunk[i]);
    flash_flag_clear(FLASH_OP_FLAGS);
    flash_lock();
    __set_PRIMASK(primask);

    if (status != FLASH_OPERATE_DONE)
      return 1;
  }

  for (uint32_t i = 0; i < len; i++)
  {
    if (*(const volatile uint8_t *)(address + i) != chunk[i])
      return 1;
  }
  return 0;
}
