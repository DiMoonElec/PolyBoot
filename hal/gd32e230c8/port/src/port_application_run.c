#include "bootloader_port.h"
#include "bootloader_project_config.h"
#include "gd32e23x.h"

__attribute__( ( naked, noreturn ) ) void BootJumpASM(uint32_t SP, uint32_t RH )
{
  __asm("MSR      MSP,r0");
  __asm("BX       r1");
}

void port_application_run(void)
{
  volatile uint32_t *Address = (volatile uint32_t *)BOOTLOADER_APP_BEGIN;
  SCB->VTOR = (uint32_t)Address;
  
  BootJumpASM(Address[0], Address[1]);
}