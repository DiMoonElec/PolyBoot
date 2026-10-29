#include "systick.h"
#include "serial_port.h"
#include "proxy_serial_port.h"

void SysTick_Handler(void)
{
  timebase_ticks++;
}

void UART8_IRQHandler(void)
{
  SerialPort_IRQHandler();
}

void UART7_IRQHandler(void)
{
  ProxySerial_IRQHandler(0);
}

void UART5_IRQHandler(void)
{
  ProxySerial_IRQHandler(1);
}
