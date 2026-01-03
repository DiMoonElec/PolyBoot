#include "bootloader_port.h"
#include "bootloader_project_config.h"
#include "gd32e23x.h"

#if 0

int port_boot_jumper_is_active(void)
{
  return gpio_input_data_bit_read(GPIOB, GPIO_PINS_12) == RESET;
}


static void deinit_all(void)
{
  // Отключение прерывание UART
  NVIC_DisableIRQ(USART1_IRQn);
  
  NVIC->ICER[0] = 0xFFFFFFFF ;
  NVIC->ICER[1] = 0xFFFFFFFF ;
  NVIC->ICER[2] = 0xFFFFFFFF ;
  NVIC->ICER[3] = 0xFFFFFFFF ;
  NVIC->ICER[4] = 0xFFFFFFFF ;
  NVIC->ICER[5] = 0xFFFFFFFF ;
  NVIC->ICER[6] = 0xFFFFFFFF ;
  NVIC->ICER[7] = 0xFFFFFFFF ;
  
  NVIC->ICPR[0] = 0xFFFFFFFF ;
  NVIC->ICPR[1] = 0xFFFFFFFF ;
  NVIC->ICPR[2] = 0xFFFFFFFF ;
  NVIC->ICPR[3] = 0xFFFFFFFF ;
  NVIC->ICPR[4] = 0xFFFFFFFF ;
  NVIC->ICPR[5] = 0xFFFFFFFF ;
  NVIC->ICPR[6] = 0xFFFFFFFF ;
  NVIC->ICPR[7] = 0xFFFFFFFF ;

  // Отключаем SysTick timer
  SysTick->CTRL = 0 ;
  SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
  
  SCB->SHCSR &= ~( SCB_SHCSR_USGFAULTENA_Msk 
                  | SCB_SHCSR_BUSFAULTENA_Msk 
                  | SCB_SHCSR_MEMFAULTENA_Msk);
  
  
  if( CONTROL_SPSEL_Msk & __get_CONTROL( ) )
  {  /* MSP is not active */
    __set_MSP( __get_PSP( ) ) ;
    __set_CONTROL( __get_CONTROL( ) & ~CONTROL_SPSEL_Msk ) ;
  }

  crm_periph_reset(CRM_USART3_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_GPIOB_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_GPIOC_PERIPH_RESET, TRUE);
  crm_periph_reset(CRM_IOMUX_PERIPH_RESET, TRUE);

  for (int i = 0; i < 10; i++)
    asm("nop");

  crm_periph_reset(CRM_USART3_PERIPH_RESET, FALSE);
  crm_periph_reset(CRM_GPIOB_PERIPH_RESET, FALSE);
  crm_periph_reset(CRM_GPIOC_PERIPH_RESET, FALSE);
  crm_periph_reset(CRM_IOMUX_PERIPH_RESET, FALSE);

  // отключаем тактирование
  crm_reset();
}



static void __jumper_gpio_init(void)
{
  gpio_init_type gpio_init_struct;

  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);

  gpio_default_para_init(&gpio_init_struct);

  /* configure the led gpio */
  gpio_init_struct.gpio_pull = GPIO_PULL_UP;
  gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
  gpio_init_struct.gpio_pins = GPIO_PINS_12;
  gpio_init(GPIOB, &gpio_init_struct);
}

#endif

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