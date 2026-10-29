#include "hw_gpio_config.h"
#include "at32f413.h"

void hw_gpio_config(void)
{
  /* PA9/PA10 настраиваются в hw_uart_config(). Другие GPIO не используются. */
}

void hw_gpio_deinit(void)
{
  crm_periph_reset(CRM_GPIOA_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_GPIOA_PERIPH_RESET, FALSE);
  crm_periph_reset(CRM_IOMUX_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_IOMUX_PERIPH_RESET, FALSE);
}
