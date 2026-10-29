#include "at32f435_437.h"
#include "hw_system_clock.h"
#include "systick.h"

extern const uint32_t __vector_table[];

void SysTick_Handler(void)
{
  timebase_ticks++;
}

void main(void)
{
  gpio_init_type gpio_init_struct;
  uint32_t last_tick = 0;
  uint8_t level = 1;

  /* SystemInit() библиотеки вернул VTOR в начало Flash.
     До включения прерываний устанавливаем таблицу этого приложения. */
  SCB->VTOR = (uint32_t)__vector_table;
  __DSB();
  __ISB();

  hw_system_clock_config();
  crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);

  gpio_bits_set(GPIOC, GPIO_PINS_13);
  gpio_default_para_init(&gpio_init_struct);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = GPIO_PINS_13;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOC, &gpio_init_struct);

  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  NVIC_SetPriority(SysTick_IRQn, 15);
  SysTick_Init();

  for (;;)
  {
    if ((uint32_t)(SYSTICK_GET_VALUE() - last_tick) >= 1000UL)
    {
      last_tick += 1000UL;
      level ^= 1;
      if (level)
        gpio_bits_set(GPIOC, GPIO_PINS_13);
      else
        gpio_bits_reset(GPIOC, GPIO_PINS_13);
    }
  }
}
