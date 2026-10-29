/*******************************************************************************
File:   RingFIFO.h
Ver     1.0
Autor:  Sivokon Dmitriy aka DiMoon Electronics
Date:   2019/06/26
*******************************************************************************/

#ifndef __RING_FIFO_H__
#define __RING_FIFO_H__

#include <stdint.h>

typedef struct 
{
  uint16_t Len;
  uint16_t Head;
  uint16_t NumOfItems;
  uint8_t *Buff;
} RingBuff_t;


//Инициализация кольцевого буфера
//
//buff - указатель на структуру буфера
//Mem - массив, в котором будет хранится буфер
//Len - Длинна массива Mem
void RingBuffInit(RingBuff_t *buff, uint8_t *Mem, uint16_t Len);


//Положить элемент в буфер
//
//buff - указатель на структуру буфера
//val - записываемое значение
//
void RingBuffPut(RingBuff_t *buff, uint8_t val);


//Получить очередное значение из кольцевого буфера
//
//buff - указатель на структуру буфера
//
//Возвращает: 
// -1 - буфер пуст
// 0..255 - значение
int16_t RingBuffGet(RingBuff_t *buff);


//Плучить количество доступных элементов в буфере
//
//buff - указатель на структуру буфера
//
//Возвращает: количество занятых элементов
uint16_t RingBuffNumOfItems(RingBuff_t *buff);

//Получить количество свободных элементов в буфере
uint16_t RingBuffNumOfFreeItems(RingBuff_t *buff);

//Удалить все элементы из буфера
//
//buff - указатель на структуру буфера
//
void RingBuffClear(RingBuff_t *buff);

                    
#endif

/******************************** END OF FILE *********************************/