#include "bootloader.h"
#include "bootloader_port.h"
#include "port_hardware.h"
#include "serial_port.h"
#include "systick.h"

int16_t port_serial_putc(uint8_t c) 
{ 
  return SerialPortPutc(c); 
}

int port_serial_transfer_completed(void) 
{ 
  return SerialPortTransferCompleted(); 
}

int16_t port_serial_getc(void) 
{ 
  return SerialPortGetc(); 
}

void port_deinit_all(void)
{
  SysTick_Deinit();
  hw_deinit();
}

int port_boot_jumper_is_active(void)
{
  return 0;
}

void main(void)
{
  SysTick_Init();
  hw_init();

  SerialPortInit();

  InitBootloader();
  
  for(;;)
  {
    ProcessBootloader();
  }
}
