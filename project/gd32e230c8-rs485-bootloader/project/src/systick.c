#include "systick.h"
#include "gd32e23x.h"

/******************************************************************************/

uint32_t SystickCounter_ms;

/******************************************************************************/

void SysTick_Init(void)
{
  SystickCounter_ms = 0;
  SysTick_Config(72000);
}

void SysTick_Deinit(void)
{
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
}

void SysTick_Handler(void)
{
  SystickCounter_ms++;
}