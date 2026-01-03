#include "gd32e23x.h"
#include "bootloader_port.h"
#include "bootloader_hal_config.h"
#include "bootloader_project_config.h"

#define FLASH_ERASE_VALUE 0xFFFFFFFFU

uint8_t port_sector_isclear(uint32_t sector)
{
  for (uint32_t i = sector; i < sector + BOOTLOADER_FLASH_SECTOR_SIZE; i += 4)
  {
    if ((*(__IO uint32_t *)(i)) != FLASH_ERASE_VALUE)
      return 0;
  }
  return 1;
}

uint8_t port_sector_erase(uint32_t page_addr)
{
  __disable_irq();

  fmc_unlock();
  fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);

  fmc_page_erase(page_addr);

  fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
  fmc_lock();

  __enable_irq();

  return 0;
}

uint8_t port_write_chunk(uint8_t *chunk,
                         uint32_t address,
                         uint16_t len)
{
  uint32_t i = 0;
  uint32_t tail;
  uint32_t word;

  for (;;)
  {
    /* сколько байт осталось записать */
    tail = len - i;

    /* все записали */
    if ((int32_t)tail <= 0)
      break;

    /* проверка выхода за границы приложения */
    if (address + tail > (BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH))
      return 1;

    __disable_irq();
    fmc_unlock();

    if (tail >= 4)
    {
      /* обычная word-запись */
      word = *(uint32_t *)(void *)(chunk + i);
      fmc_word_program(address, word);

      address += 4;
      i += 4;
    }
    else
    {
      /* заполняем всё 0xFF */
      word = 0xFFFFFFFFU;

      uint8_t *p = (uint8_t *)&word;

      /* копируем хвост */
      for (uint32_t b = 0; b < tail; b++)
      {
        p[b] = chunk[i + b];
      }

      fmc_word_program(address, word);

      address += 4;
      i += tail;
    }

    fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
    fmc_lock();
    __enable_irq();
  }

  /* Верификация */
  for (uint32_t j = 0; j < len; j++)
  {
    if (*(uint8_t *)(address - len + j) != chunk[j])
      return 1;
  }

  return 0;
}

