#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#include <stdint.h>

extern uint32_t SystickCounter_ms;

void SysTick_Init(void);
void SysTick_Deinit(void);

#define SYSTICK_GET_VALUE()     (SystickCounter_ms)

#endif
