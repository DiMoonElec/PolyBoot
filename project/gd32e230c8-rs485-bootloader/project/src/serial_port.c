#include "serial_port.h"
#include "RingFIFO.h"
#include "gd32e23x.h"

/******************************************************************************/

#define USARTx USART0
#define USARTx_IRQn USART0_IRQn
#define USARTx_IRQHandler USART0_IRQHandler

#define FIFOBUFSIZE_RX 128
#define FIFOBUFSIZE_TX 128

/******************************************************************************/

static RingBuff_t fifo_rx;
static RingBuff_t fifo_tx;

static uint8_t buff_rx[FIFOBUFSIZE_RX];
static uint8_t buff_tx[FIFOBUFSIZE_TX];

static uint8_t flag_tx_uart = 0;

/******************************************************************************/

void SerialPortInit(void)
{
  RingBuffInit(&fifo_rx, buff_rx, FIFOBUFSIZE_RX);
  RingBuffInit(&fifo_tx, buff_tx, FIFOBUFSIZE_TX);
}

int16_t SerialPortPutc(uint8_t c)
{
  int16_t ret = c;
  
  NVIC_DisableIRQ(USARTx_IRQn);
  
  if(RingBuffNumOfFreeItems(&fifo_tx) > 0)
  {
    RingBuffPut(&fifo_tx, c);
    flag_tx_uart = 1;
    usart_interrupt_enable(USARTx, USART_INT_TBE);
  }
  else 
    ret = -1;
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return ret;
}

int16_t SerialPortGetc(void)
{
  int16_t ret = 0;
  
  NVIC_DisableIRQ(USARTx_IRQn);
  
  ret = RingBuffGet(&fifo_rx);
  
  NVIC_EnableIRQ(USARTx_IRQn);
  
  return ret;
}

int SerialPortTransferCompleted(void)
{
  return RingBuffNumOfItems(&fifo_tx) == 0;
}

/******************************************************************************/

void USARTx_IRQHandler(void)
{
  // Если что-то получили по uart
  if (usart_flag_get(USARTx, USART_FLAG_RBNE) == SET)
    RingBuffPut(&fifo_rx, (uint8_t)usart_data_receive(USARTx));

  // Если отправка данных включена, и буфер передатчика пуст
  if (flag_tx_uart && (usart_flag_get(USARTx, USART_FLAG_TBE) == SET))
  {
    if (RingBuffNumOfItems(&fifo_tx) > 0) // если есть, что передавать
    {
      usart_data_transmit(USARTx, RingBuffGet(&fifo_tx));
    }
    else
    {
      flag_tx_uart = 0;
      usart_interrupt_disable(USARTx, USART_INT_TBE);
    }
  }
}
