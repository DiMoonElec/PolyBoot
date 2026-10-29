#include "hw_uart_config.h"
#include "bootloader_project_config.h"
#include "at32f435_437.h"

void hw_uart_config(void)
{
  gpio_init_type gpio_init_struct;
  gpio_default_para_init(&gpio_init_struct);

  /* PC2 — TX. */
  gpio_pin_mux_config(GPIOC, GPIO_PINS_SOURCE2, GPIO_MUX_8);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_2;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOC, &gpio_init_struct);

  /* PC3 — RX. */
  gpio_pin_mux_config(GPIOC, GPIO_PINS_SOURCE3, GPIO_MUX_8);
  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
  gpio_init_struct.gpio_pins = GPIO_PINS_3;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOC, &gpio_init_struct);

  /* configure param */
  usart_init(UART8, BOOTLOADER_UART_BAUD, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_transmitter_enable(UART8, TRUE);
  usart_receiver_enable(UART8, TRUE);
  usart_parity_selection_config(UART8, USART_PARITY_NONE);

  usart_hardware_flow_control_set(UART8, USART_HARDWARE_FLOW_NONE);

  /* Приём работает по IRQ; передачу TDBE включает драйвер. */
  usart_interrupt_enable(UART8, USART_RDBF_INT, TRUE);
  usart_interrupt_enable(UART8, USART_TDBE_INT, FALSE);
  usart_enable(UART8, TRUE);
}

void hw_uart_deinit(void)
{
  crm_periph_reset(CRM_UART8_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_UART8_PERIPH_RESET, FALSE);
}
