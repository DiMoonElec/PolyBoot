#include "at32f413.h"
#include "hw_config.h"
#include "hw_gpio_config.h"
#include "hw_uart_config.h"
#include "systick.h"

static void __periph_clock_init(void)
{
  crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, TRUE);
}

static void __periph_clock_deinit(void)
{
  crm_periph_clock_enable(CRM_USART1_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_IOMUX_PERIPH_CLOCK, FALSE);
}

static void __nvic_init(void)
{
  nvic_priority_group_config(NVIC_PRIORITY_GROUP_4);
  NVIC_SetPriority(SysTick_IRQn, 15);
  nvic_irq_enable(USART1_IRQn, 0, 0);
}

static void __nvic_deinit(void)
{
  NVIC_DisableIRQ(USART1_IRQn);
  NVIC_ClearPendingIRQ(USART1_IRQn);
  NVIC_SetPriority(USART1_IRQn, 0);
  NVIC_SetPriority(SysTick_IRQn, 0);
  NVIC_SetPriorityGrouping(0);
}

void hw_config(void)
{
  __periph_clock_init();
  hw_gpio_config();
  hw_uart_config();
  __nvic_init();
  SysTick_Init();
}

void hw_deinit(void)
{
  __nvic_deinit();
  SysTick_Deinit();
  hw_uart_deinit();
  /* Сброс UART мог снять запрос прерывания; убираем оставшийся pending. */
  NVIC_ClearPendingIRQ(USART1_IRQn);
  hw_gpio_deinit();
  __periph_clock_deinit();
}
