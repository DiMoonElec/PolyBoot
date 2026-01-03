/******************************************************************************
File:   binex-lib.h
Ver     1.0
Date:   2023/04/30
Autor:  Sivokon Dmitriy aka DiMoon Electronics
*******************************************************************************
BSD 2-Clause License
Copyright (c) 2023, Sivokon Dmitriy aka DiMoon Electronics
All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

/******************************************************************************
  Данная библиотека позволяет организовать пакетную передачу данных 
  через посимвольный канал связи, например, через UART.
  Формат передаваемых данных следующий:
  +--------------+-------+-------------------+--------+------------------------+
  |номер элемента|   0   | 1,2 (uint16, lsb) |  3..n  | n+1, n+2 (uint16, lsb) |
  +--------------+-------+-------------------+--------+------------------------+
  |  значение    | START |    PACK_LEN       |  DATA  |         CRC16          |
  +--------------+-------+-------------------+--------+------------------------+

  О начале пакета символизирует код START (BINEX_START_SYMBOL). Затем идет 
  размер передаваемых данных, который состоит из 2х байт в формате LSB. После 
  этого следуют полезные данные DATA. В конце передается контрольная сумма 
  пакета, которая вычисляется для полей PACK_LEN и DATA.
  
  Если в поле PACK_LEN, DATA или CRC16 встречается код символа START, то он 
  экранируется символом ESC (BINEX_ESC_SYMBOL). Если в этих же даннх встречается
  код символа ESC, то он также экранируется ESC-символом. Иными словами, символ 
  START будет заменен последовательностью ESC START, а символ ESC будет заменен 
  на ESC ESC.
  При приеме данных контрольная сумма вычисляется уже после замены 
  ESC-последовательности, а при передаче, до замены на ESC-последовательности.
  Управляющие символы можно выбрать произвольными, для этого необходимо 
  исправить значения define-ов BINEX_START_SYMBOL BINEX_ESC_SYMBOL. Главное, 
  чтоб эти символы не были одинаковыми. Если не объявлен define BINEX_CHECK_CRC,
  то контрольные суммы при передаче не будут вычисляться, и поле CRC16 будет 
  отсутствовать. При приеме поле CRC16 может отсутстовать, и оно будет 
  игнорироваться, если присутствует.
******************************************************************************/

#ifndef __BINEX_LIB_H__
#define __BINEX_LIB_H__

#include <stdint.h>
#include <stddef.h>

//Если надо использовать CRC, то
//необходимо объявить define BINEX_CHECK_CRC
#define BINEX_CHECK_CRC

//символ начала пакета
#define BINEX_START_SYMBOL 0xF5

//esc-символ
#define BINEX_ESC_SYMBOL 0xF4

typedef enum
{
  BINEX_PACK_NOT_RX = 0, //Пакет еще не принят
  BINEX_PACK_RX = 1,     //Пакет принят
  BINEX_PACK_BROKEN = 2  //Пакет поврежден
} BinexRxStatus_t;

typedef enum
{
  BINEX_PACK_NOT_TX = 0, //Пакет еще не отправлен
  BINEX_PACK_TX = 1      //Пакет отправлен
} BinexTxStatus_t;


//Буфер приемника, его размер равен BINEX_BUFFER_SIZE
extern uint8_t binex_receive_buffer[];

// Инициализация конечного автомата приемника
void binex_receiver_begin(uint8_t *buff, size_t buff_size);

// Эту фунцию надо вызывать для каждого символа
// входного потока. Как только будет задетектирован 
// корректный пакет, функция возвратит BINEX_PACK_RX
// Если функция задетектировала поврежденный пакет, то
// будет возвращено BINEX_PACK_BROKEN.
// Длину полученного пакета можно получить с помощью
// функции binex_get_rxpack_len. Принятые данные будут
// находиться в буфере binex_receive_buffer 
BinexRxStatus_t binex_receiver(int16_t c);

//Получить длину принятого пакета
uint16_t binex_get_rxpack_len(void);

// Инициализация процесса передачи буфера
void binex_transmitter_init(void *buff, uint16_t size);

// Сам процесс передачи буфера. Эту функцию 
// нужно вызывать до тех пор, пока она не вернет
// BINEX_PACK_TX. Если вывод выполняется в поток,
// который не может переполниться, то будет достаточно
// одного вызова этой функции. Если вывод выполняется в 
// поток, который может переполниться (например, uart),
// то функция возвратит BINEX_PACK_NOT_TX, и ее надо 
// будет вызвать еще раз, через некоторое время
BinexTxStatus_t binex_transmit(void);

// Call-back функция, через которую выполняется 
// вывод данных в поток. Если в данный момент
// поток не может принять очередной символ, то функция
// должна возвратить 0. Это приведет к тому, что 
// функция binex_transmit вернет BINEX_PACK_NOT_TX,
// и binex_transmit нужно будет вызвать еще раз 
// через некоторое время для продолжения процесса.
// Если в данный момент поток может принять очередной символ,
// то этот символ должен быть отправлен в поток,
// а функция должна вернуть 1.
extern int binex_tx_callback(uint8_t c);

#endif