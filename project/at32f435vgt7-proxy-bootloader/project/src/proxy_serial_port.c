#include "at32f435_437.h"
#include "proxy_serial_port.h"
#include "RingFIFO.h"

/* Для F413: 116 событий стирания. Верхняя оценка Binex с экранированием:
   116 * (1 + 2 * (2 + 10 + 2)) + (1 + 2 * (2 + 2 + 2)) = 3377 байт.
   16 Кбайт на канал сохраняют всю серию даже без чтения FIFO во время стирания.
   Для других слейвов/наборов команд размер требуется пересчитать. */
#define PROXY_RX_SIZE 16384
#define PROXY_TX_SIZE 128

typedef struct
{
  RingBuff_t rx;
  RingBuff_t tx;
  volatile uint8_t tx_active;
  volatile uint8_t error;
} ProxySerial_t;

static ProxySerial_t serial[2];
static uint8_t rx_buffer[2][PROXY_RX_SIZE];
static uint8_t tx_buffer[2][PROXY_TX_SIZE];

void ProxySerialInit(uint8_t channel)
{
  RingBuffInit(&serial[channel].rx, rx_buffer[channel], PROXY_RX_SIZE);
  RingBuffInit(&serial[channel].tx, tx_buffer[channel], PROXY_TX_SIZE);
  serial[channel].tx_active = 0;
  serial[channel].error = 0;
}

int16_t ProxySerialPutc(uint8_t channel, uint8_t c)
{
  int16_t result = -1;
  uint32_t primask = __get_PRIMASK();
  usart_type *uart = channel == 0 ? UART7 : UART5;

  __disable_irq();
  if (!serial[channel].error && RingBuffNumOfFreeItems(&serial[channel].tx) != 0)
  {
    RingBuffPut(&serial[channel].tx, c);
    serial[channel].tx_active = 1;
    usart_interrupt_enable(uart, USART_TDBE_INT, TRUE);
    result = c;
  }
  __set_PRIMASK(primask);
  return result;
}

int16_t ProxySerialGetc(uint8_t channel)
{
  int16_t result = -1;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (!serial[channel].error)
    result = RingBuffGet(&serial[channel].rx);
  __set_PRIMASK(primask);
  return result;
}

int ProxySerialTransferCompleted(uint8_t channel)
{
  usart_type *uart = channel == 0 ? UART7 : UART5;
  return RingBuffNumOfItems(&serial[channel].tx) == 0 &&
         usart_flag_get(uart, USART_TDC_FLAG) == SET;
}

uint8_t ProxySerialHasError(uint8_t channel)
{
  return serial[channel].error;
}

void ProxySerial_IRQHandler(uint8_t channel)
{
  usart_type *uart = channel == 0 ? UART7 : UART5;
  uint32_t status = uart->sts;

  if (status & (USART_ROERR_FLAG | USART_FERR_FLAG | USART_NERR_FLAG | USART_PERR_FLAG))
    serial[channel].error = 1;

  /* Чтение sts выше и dt ниже также снимает аппаратные ошибки приёмника. */
  if (status & (USART_RDBF_FLAG | USART_ROERR_FLAG | USART_FERR_FLAG | USART_NERR_FLAG | USART_PERR_FLAG))
  {
    uint8_t c = (uint8_t)usart_data_receive(uart);
    if (!serial[channel].error && RingBuffNumOfFreeItems(&serial[channel].rx) != 0)
      RingBuffPut(&serial[channel].rx, c);
    else
      serial[channel].error = 1;
  }

  if (serial[channel].tx_active && (status & USART_TDBE_FLAG))
  {
    if (RingBuffNumOfItems(&serial[channel].tx) != 0)
      usart_data_transmit(uart, (uint16_t)RingBuffGet(&serial[channel].tx));
    else
    {
      serial[channel].tx_active = 0;
      usart_interrupt_enable(uart, USART_TDBE_INT, FALSE);
    }
  }
}
