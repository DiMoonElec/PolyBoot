#include "at32f413.h"
#include "bootloader_port.h"
#include "bootloader_project_config.h"

/* IAR: после смены MSP нельзя обращаться к стеку загрузчика. */
__stackless static void boot_jump(uint32_t sp, uint32_t reset_handler)
{
  __asm("MSR MSP, R0");
  __asm("BX R1");
}

void port_application_run(void)
{
  const uint32_t *vectors = (const uint32_t *)BOOTLOADER_APP_BEGIN;

  /* Периферия и её IRQ уже отключены в port_deinit_all().
     VTOR приложение устанавливает само до включения своих прерываний. */
  boot_jump(vectors[0], vectors[1]);
}
