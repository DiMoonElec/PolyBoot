#include "hw_gpio_config.h"
#include "at32f435_437.h"

void hw_gpio_config(void)
{
  /* PC2/PC3 настраиваются в hw_uart_config(). */
}

void hw_gpio_deinit(void)
{
  gpio_reset(GPIOC);
}
