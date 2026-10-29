#ifndef __PROXY_SERIAL_PORT_H__
#define __PROXY_SERIAL_PORT_H__

#include <stdint.h>

/* 0 — UART7, 1 — UART5. Сбрасывать FIFO только при отключённом UART IRQ. */
void ProxySerialInit(uint8_t channel);
int16_t ProxySerialPutc(uint8_t channel, uint8_t c);
int16_t ProxySerialGetc(uint8_t channel);
int ProxySerialTransferCompleted(uint8_t channel);
uint8_t ProxySerialHasError(uint8_t channel);
void ProxySerial_IRQHandler(uint8_t channel);

#endif
