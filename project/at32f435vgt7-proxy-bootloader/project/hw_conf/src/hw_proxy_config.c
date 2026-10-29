#include "at32f435_437.h"
#include "hw_proxy_config.h"

typedef struct
{
  gpio_type *gpio;
  uint16_t pin;
} SlaveCs_t;

/* ID 0..11 соответствуют физическим CS1..CS12. */
static const SlaveCs_t slave_cs[PROXY_SLAVE_COUNT] = {
    {GPIOD, GPIO_PINS_2}, {GPIOD, GPIO_PINS_3},
    {GPIOD, GPIO_PINS_4}, {GPIOD, GPIO_PINS_5},
    {GPIOD, GPIO_PINS_6}, {GPIOD, GPIO_PINS_7},
    {GPIOE, GPIO_PINS_6}, {GPIOE, GPIO_PINS_5},
    {GPIOE, GPIO_PINS_4}, {GPIOE, GPIO_PINS_3},
    {GPIOE, GPIO_PINS_2}, {GPIOE, GPIO_PINS_1}};

#define CS_D_PINS (GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 | GPIO_PINS_7)
#define CS_E_PINS (GPIO_PINS_1 | GPIO_PINS_2 | GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6)
#define BUS_PINS (GPIO_PINS_3 | GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6)

void hw_proxy_deselect_all(void)
{
  gpio_bits_reset(GPIOD, CS_D_PINS);
  gpio_bits_reset(GPIOE, CS_E_PINS);
}

void hw_proxy_select(uint8_t slave_id)
{
  gpio_bits_set(slave_cs[slave_id].gpio, slave_cs[slave_id].pin);
}

void hw_proxy_reset_slaves(uint8_t asserted)
{
  if (asserted)
  {
    gpio_bits_reset(GPIOA, GPIO_PINS_15);
    gpio_bits_reset(GPIOE, GPIO_PINS_0);
  }
  else
  {
    gpio_bits_set(GPIOA, GPIO_PINS_15);
    gpio_bits_set(GPIOE, GPIO_PINS_0);
  }
}

void hw_proxy_gpio_config(void)
{
  gpio_init_type gpio;

  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_UART7_PERIPH_CLOCK, TRUE);
  crm_periph_clock_enable(CRM_UART5_PERIPH_CLOCK, TRUE);

  hw_proxy_deselect_all();
  hw_proxy_reset_slaves(1);
  gpio_default_para_init(&gpio);
  gpio.gpio_mode = GPIO_MODE_OUTPUT;
  gpio.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio.gpio_pull = GPIO_PULL_NONE;
  gpio.gpio_pins = GPIO_PINS_15;
  gpio_init(GPIOA, &gpio);
  gpio.gpio_pins = CS_D_PINS;
  gpio_init(GPIOD, &gpio);
  gpio.gpio_pins = CS_E_PINS | GPIO_PINS_0;
  gpio_init(GPIOE, &gpio);

  /* Линии шины пока отпущены; два нуля будут выставлены после паузы RESET. */
  gpio.gpio_mode = GPIO_MODE_INPUT;
  gpio.gpio_pins = BUS_PINS;
  gpio_init(GPIOB, &gpio);
}

void hw_proxy_lines_low(void)
{
  gpio_init_type gpio;
  gpio_bits_reset(GPIOB, BUS_PINS);
  gpio_default_para_init(&gpio);
  gpio.gpio_mode = GPIO_MODE_OUTPUT;
  gpio.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio.gpio_pull = GPIO_PULL_NONE;
  gpio.gpio_pins = BUS_PINS;
  gpio_init(GPIOB, &gpio);
}

void hw_proxy_uart_pins_config(void)
{
  gpio_init_type gpio;
  gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE3, GPIO_MUX_8);
  gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE4, GPIO_MUX_8);
  gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE5, GPIO_MUX_8);
  gpio_pin_mux_config(GPIOB, GPIO_PINS_SOURCE6, GPIO_MUX_8);
  gpio_default_para_init(&gpio);
  gpio.gpio_mode = GPIO_MODE_MUX;
  gpio.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio.gpio_pull = GPIO_PULL_NONE;
  gpio.gpio_pins = BUS_PINS;
  gpio_init(GPIOB, &gpio);
}

void hw_proxy_uart_config(uint8_t channel)
{
  usart_type *uart = channel == 0 ? UART7 : UART5;
  IRQn_Type irq = channel == 0 ? UART7_IRQn : UART5_IRQn;

  usart_reset(uart);
  usart_init(uart, 115200, USART_DATA_8BITS, USART_STOP_1_BIT);
  usart_parity_selection_config(uart, USART_PARITY_NONE);
  usart_hardware_flow_control_set(uart, USART_HARDWARE_FLOW_NONE);
  /* SWAP меняет функции ног: TX становится PB3/PB5, RX — PB4/PB6. */
  usart_transmit_receive_pin_swap(uart, TRUE);
  usart_transmitter_enable(uart, TRUE);
  usart_receiver_enable(uart, TRUE);
  usart_interrupt_enable(uart, USART_RDBF_INT, TRUE);
  usart_interrupt_enable(uart, USART_TDBE_INT, FALSE);
  NVIC_ClearPendingIRQ(irq);
  nvic_irq_enable(irq, 0, 0);
  usart_enable(uart, TRUE);
}

void hw_proxy_uart_deinit(uint8_t channel)
{
  usart_type *uart = channel == 0 ? UART7 : UART5;
  IRQn_Type irq = channel == 0 ? UART7_IRQn : UART5_IRQn;

  NVIC_DisableIRQ(irq);
  usart_reset(uart);
  NVIC_ClearPendingIRQ(irq);
  NVIC_SetPriority(irq, 0);
}

void hw_proxy_deinit(void)
{
  hw_proxy_deselect_all();
  hw_proxy_uart_deinit(0);
  hw_proxy_uart_deinit(1);
  hw_proxy_reset_slaves(0);
  crm_periph_clock_enable(CRM_UART7_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_UART5_PERIPH_CLOCK, FALSE);

  /* В демо эти порты принадлежат только прокси. UART8 использует GPIOC. */
  gpio_reset(GPIOA);
  gpio_reset(GPIOB);
  gpio_reset(GPIOD);
  gpio_reset(GPIOE);
  crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, FALSE);
  crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, FALSE);
}
