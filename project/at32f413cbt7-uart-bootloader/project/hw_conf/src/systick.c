#include "systick.h"
#include "at32f413.h"

volatile uint32_t timebase_ticks;

void SysTick_Init(void)
{
  timebase_ticks = 0;
  systick_clock_source_config(SYSTICK_CLOCK_SOURCE_AHBCLK_DIV8);
  SysTick->LOAD = 25000UL - 1UL;
  SysTick->VAL = 0;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

void SysTick_Deinit(void)
{
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
  timebase_ticks = 0;
}
