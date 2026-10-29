#include "serial_port.h"
#include "RingFIFO.h"
#include "at32f413.h"

#define FIFOBUFSIZE_RX 128
#define FIFOBUFSIZE_TX 128

static RingBuff_t fifo_rx;
static RingBuff_t fifo_tx;
static uint8_t buff_rx[FIFOBUFSIZE_RX];
static uint8_t buff_tx[FIFOBUFSIZE_TX];
static volatile uint8_t flag_tx_uart;

void SerialPortInit(void)
{
  RingBuffInit(&fifo_rx, buff_rx, sizeof(buff_rx));
  RingBuffInit(&fifo_tx, buff_tx, sizeof(buff_tx));
  flag_tx_uart = 0;
}

int16_t SerialPortPutc(uint8_t c)
{
  int16_t result = -1;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (RingBuffNumOfFreeItems(&fifo_tx) != 0)
  {
    RingBuffPut(&fifo_tx, c);
    flag_tx_uart = 1;
    usart_interrupt_enable(USART1, USART_TDBE_INT, TRUE);
    result = c;
  }
  __set_PRIMASK(primask);
  return result;
}

int16_t SerialPortGetc(void)
{
  int16_t result;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  result = RingBuffGet(&fifo_rx);
  __set_PRIMASK(primask);
  return result;
}

int SerialPortTransferCompleted(void)
{
  /* Пустого FIFO недостаточно: последний байт ещё может передаваться. */
  return (RingBuffNumOfItems(&fifo_tx) == 0) &&
         (usart_flag_get(USART1, USART_TDC_FLAG) == SET);
}

void SerialPort_IRQHandler(void)
{
  if (usart_flag_get(USART1, USART_RDBF_FLAG) == SET)
  {
    uint8_t c = (uint8_t)usart_data_receive(USART1);
    if (RingBuffNumOfFreeItems(&fifo_rx) != 0)
      RingBuffPut(&fifo_rx, c);
  }

  if (flag_tx_uart && (usart_flag_get(USART1, USART_TDBE_FLAG) == SET))
  {
    if (RingBuffNumOfItems(&fifo_tx) != 0)
      usart_data_transmit(USART1, (uint16_t)RingBuffGet(&fifo_tx));
    else
    {
      flag_tx_uart = 0;
      usart_interrupt_enable(USART1, USART_TDBE_INT, FALSE);
    }
  }
}
