#include "systick.h"
#include "serial_port.h"

void SysTick_Handler(void)
{
  timebase_ticks++;
}

void USART1_IRQHandler(void)
{
  SerialPort_IRQHandler();
}
