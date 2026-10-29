#include "bootloader_port.h"
#include "hw_config.h"
#include "serial_port.h"
#include "hw_system_clock.h"

int16_t port_serial_putc(uint8_t c)
{
  return SerialPortPutc(c);
}

int16_t port_serial_getc(void)
{
  return SerialPortGetc();
}

int port_serial_transfer_completed(void)
{
  return SerialPortTransferCompleted();
}

int port_boot_jumper_is_active(void)
{
  /* Демо активируется командой UART в течение первых 10 секунд. */
  return 0;
}

void port_deinit_all(void)
{
  hw_deinit();
  SerialPortInit();
  hw_system_clock_deinit();
}
