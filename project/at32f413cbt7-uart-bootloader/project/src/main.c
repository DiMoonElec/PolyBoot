#include "bootloader.h"
#include "hw_config.h"
#include "serial_port.h"
#include "system_clock.h"

void main(void)
{
  wk_system_clock_config();
  SerialPortInit();
  hw_config();
  InitBootloader();

  for (;;)
  {
    ProcessBootloader();
  }
}
