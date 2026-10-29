#include "bootloader_proxy_port.h"
#include "hw_proxy_config.h"
#include "proxy_serial_port.h"
#include "systick.h"

#define SELECT_PAUSE_MS 10UL

enum
{
  INIT_RESET,
  INIT_LINES_LOW,
  INIT_BOOT_WAIT,
  INIT_FINISHED
};

static uint8_t hardware_active;
static uint8_t selected_channel;
static uint8_t target_id;
static uint8_t select_phase;
static ProxyPortResult_t select_result;
static ProxyPortResult_t close_result;
static uint32_t select_timer;
static uint32_t close_timer;
static uint8_t init_phase;
static uint32_t init_timer;

static void stop_uarts(void)
{
  hw_proxy_uart_deinit(0);
  hw_proxy_uart_deinit(1);
}

void port_proxy_init_begin(void)
{
  if (hardware_active)
  {
    hw_proxy_deselect_all();
    stop_uarts();
  }
  hw_proxy_gpio_config();
  hardware_active = 1;
  init_timer = SYSTICK_GET_VALUE();
  init_phase = INIT_RESET;
}

ProxyPortResult_t port_proxy_init_poll(void)
{
  switch (init_phase)
  {
  case INIT_RESET:
    /* При SysTick 1 мс два тика гарантируют паузу не менее 1 мс. */
    if ((uint32_t)(SYSTICK_GET_VALUE() - init_timer) >= 2UL)
    {
      hw_proxy_lines_low();
      init_timer = SYSTICK_GET_VALUE();
      init_phase = INIT_LINES_LOW;
    }
    break;

  case INIT_LINES_LOW:
    if ((uint32_t)(SYSTICK_GET_VALUE() - init_timer) >= 2UL)
    {
      hw_proxy_reset_slaves(0);
      init_timer = SYSTICK_GET_VALUE();
      init_phase = INIT_BOOT_WAIT;
    }
    break;

  case INIT_BOOT_WAIT:
    if ((uint32_t)(SYSTICK_GET_VALUE() - init_timer) >= 11UL)
    {
      hw_proxy_uart_pins_config();
      ProxySerialInit(0);
      ProxySerialInit(1);
      hw_proxy_uart_config(0);
      hw_proxy_uart_config(1);
      init_phase = INIT_FINISHED;
    }
    break;

  case INIT_FINISHED:
    return PROXY_PORT_DONE;
  }
  return PROXY_PORT_BUSY;
}

ProxyListResult_t port_proxy_get_slave_list(uint16_t offset, uint8_t *ids,
                                          uint16_t capacity, uint16_t *count)
{
  /* Плата имеет 12 позиций. Наличие загрузчика проверяет ПК через A2/A4. */
  *count = 0;
  while (offset < PROXY_SLAVE_COUNT && *count < capacity)
    ids[(*count)++] = (uint8_t)offset++;
  return offset < PROXY_SLAVE_COUNT ? PROXY_LIST_MORE : PROXY_LIST_DONE;
}

void port_proxy_open_begin(uint8_t slave_id)
{
  hw_proxy_deselect_all();
  stop_uarts();
  select_result = PROXY_PORT_ERROR;
  if (slave_id >= PROXY_SLAVE_COUNT)
    return;

  target_id = slave_id;
  selected_channel = slave_id < 6 ? 0 : 1;
  select_phase = 0;
  select_timer = SYSTICK_GET_VALUE();
  select_result = PROXY_PORT_BUSY;
}

ProxyPortResult_t port_proxy_open_poll(void)
{
  if (select_result != PROXY_PORT_BUSY)
    return select_result;

  if ((uint32_t)(SYSTICK_GET_VALUE() - select_timer) < SELECT_PAUSE_MS)
    return PROXY_PORT_BUSY;

  if (select_phase == 0)
  {
    ProxySerialInit(selected_channel);
    hw_proxy_uart_config(selected_channel);
    hw_proxy_select(target_id);
    select_timer = SYSTICK_GET_VALUE();
    select_phase = 1;
  }
  else
    select_result = PROXY_PORT_DONE;
  return select_result;
}

void port_proxy_close_begin(void)
{
  close_result = PROXY_PORT_DONE;
  if (!hardware_active)
    return;

  hw_proxy_deselect_all();
  stop_uarts();
  hw_proxy_reset_slaves(0);
  /* CLOSE может отменять INIT, пока линии ещё были выходами с нулём. */
  hw_proxy_uart_pins_config();
  close_timer = SYSTICK_GET_VALUE();
  close_result = PROXY_PORT_BUSY;
}

ProxyPortResult_t port_proxy_close_poll(void)
{
  if (close_result == PROXY_PORT_BUSY &&
      (uint32_t)(SYSTICK_GET_VALUE() - close_timer) >= SELECT_PAUSE_MS)
    close_result = PROXY_PORT_DONE;
  return close_result;
}

void port_proxy_deinit(void)
{
  if (hardware_active)
  {
    hw_proxy_deinit();
    hardware_active = 0;
  }
}

int16_t port_proxy_serial_putc(uint8_t c)
{
  return ProxySerialPutc(selected_channel, c);
}

int16_t port_proxy_serial_getc(void)
{
  return ProxySerialGetc(selected_channel);
}

int port_proxy_serial_transfer_completed(void)
{
  return ProxySerialTransferCompleted(selected_channel);
}
