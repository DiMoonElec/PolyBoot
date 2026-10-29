#include "systick.h"
#include "serial_port.h"

void SysTick_Handler(void)
{
  timebase_ticks++;
}

void UART8_IRQHandler(void)
{
  SerialPort_IRQHandler();
}
