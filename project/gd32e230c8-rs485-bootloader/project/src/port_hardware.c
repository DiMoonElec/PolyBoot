#include "bootloader_port.h"
#include "bootloader_project_config.h"
#include "gd32e23x.h"

/*************************************************************************/

static void __rcu_deinit(void)
{
  rcu_periph_clock_disable(RCU_GPIOA);
  rcu_periph_clock_disable(RCU_USART0);
}

static void __gpio_deinit(void)
{
  gpio_deinit(GPIOA);
}

static void __uart_deinit(void)
{
  usart_deinit(USART0);
}

static void __nvic_deinit(void)
{
  nvic_irq_disable(USART0_IRQn);
}

/*************************************************************************/

static void __rcu_init(void)
{
  /* enable COM GPIO clock */
  rcu_periph_clock_enable(RCU_GPIOA);

  /* enable USART clock */
  rcu_periph_clock_enable(RCU_USART0);
}

static void __gpio_init(void)
{
  /* connect port to USARTx_Tx */
  gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_9);

  /* connect port to USARTx_Rx */
  gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_10);

  /* connect port to USARTx_DE */
  gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_12);

  /* configure USART Tx as alternate function push-pull */
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_9);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_9);

  /* configure USART Rx as alternate function push-pull */
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_10);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_10);

  /* configure USART DEx as alternate function push-pull */
  gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_12);
  gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_12);
}

static void __uart_init(void)
{
  /* USART configure */
  usart_deinit(USART0);

  usart_word_length_set(USART0, USART_WL_8BIT);
  usart_stop_bit_set(USART0, USART_STB_1BIT);
  usart_parity_config(USART0, USART_PM_NONE);

  usart_baudrate_set(USART0, BOOTLOADER_UART_BAUD);

  usart_rs485_driver_enable(USART0);
  usart_driver_assertime_config(USART0, 0x01);
  usart_driver_deassertime_config(USART0, 0x01);

  /* Rx/Tx swap, это из-за того, что перепутал rx и tx на плате */
  usart_invert_config(USART0, USART_SWAP_ENABLE);

  // Сбрасываем флаги прерываний
  // usart_interrupt_flag_clear(USART0, USART_INT_FLAG_RT);

  /* USART_INT_RBNE: read data buffer not empty interrupt and
    overrun error interrupt enable interrupt */
  usart_interrupt_enable(USART0, USART_INT_RBNE);

  usart_transmit_config(USART0, USART_TRANSMIT_ENABLE);
  usart_receive_config(USART0, USART_RECEIVE_ENABLE);
  usart_enable(USART0);
}

static void __nvic_init(void)
{
  nvic_irq_enable(USART0_IRQn, 0);
}

/*************************************************************************/

void hw_init(void)
{
  __rcu_init();
  __gpio_init();
  __uart_init();
  __nvic_init();
}

void hw_deinit(void)
{
  __nvic_deinit();
  __uart_deinit();
  __gpio_deinit();
  __rcu_deinit();
}