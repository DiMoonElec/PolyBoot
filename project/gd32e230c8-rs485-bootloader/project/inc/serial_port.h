#ifndef __SERIAL_PORT_H__
#define __SERIAL_PORT_H__

#include <stdint.h>

void SerialPortInit(void);

int16_t SerialPortPutc(uint8_t c);
int16_t SerialPortGetc(void);
int SerialPortTransferCompleted(void);


#endif