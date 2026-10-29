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


/* Состояние одного канала. Буферы предоставляет вызывающий код.
   Каждый экземпляр обслуживается последовательно, без одновременных
   вызовов из основного цикла и прерывания. */
typedef struct
{
  uint8_t *receive_buffer;
  size_t receive_buffer_size;
  uint8_t *txbuff;
  int (*tx_callback)(uint8_t c);

  uint16_t rxpack_size;
  uint16_t rx_tmp;
  uint16_t txpack_size;
  uint16_t tx_tmp;
#ifdef BINEX_CHECK_CRC
  uint16_t tx_crc16;
#endif
  uint8_t rxstate;
  uint8_t flag_prev_rx_esc;
  uint8_t txstate;
  uint8_t flag_prev_tx_esc;
} Binex_t;

/* Инициализация экземпляра. При включённом CRC перед началом обмена
   один раз вызвать Crc16Init() для всех экземпляров.
   tx_callback возвращает 1, если символ принят в выходной поток,
   и 0, если поток заполнен: передача продолжится при следующем вызове.
   Функция вывода закрепляется за экземпляром до следующего binex_init(). */
void binex_init(Binex_t *binex, int (*tx_callback)(uint8_t c));

/* Начать приём в предоставленный буфер, сбросив незавершённый пакет.
   Разные одновременно работающие приёмники используют разные буферы. */
void binex_receiver_begin(Binex_t *binex, uint8_t *buff, size_t buff_size);

/* Обработать очередной символ; отрицательное значение означает отсутствие
   данных. После BINEX_PACK_RX payload находится в буфере, переданном
   binex_receiver_begin(). До обработки следующего пакета его нужно прочитать. */
BinexRxStatus_t binex_receiver(Binex_t *binex, int16_t c);

/* Длина принятого payload. */
uint16_t binex_get_rxpack_len(const Binex_t *binex);

/* Начать передачу. Буфер должен оставаться доступным и неизменным
   до получения BINEX_PACK_TX. Приём и передача одного экземпляра
   могут обслуживаться независимо, если используют разные буферы. */
void binex_transmitter_init(Binex_t *binex, void *buff, uint16_t size);

/* Продолжать передачу до BINEX_PACK_TX. При заполнении выходного потока
   возвращает BINEX_PACK_NOT_TX без ожидания его освобождения. */
BinexTxStatus_t binex_transmit(Binex_t *binex);

#endif
