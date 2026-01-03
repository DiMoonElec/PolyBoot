/******************************************************************************
File:   binex-lib.c
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

#include "binex-lib.h"

#ifdef BINEX_CHECK_CRC
#include "crc16.h"
#endif

/******************************************************************************/

#define BINEX_ESCAPE 0x00
#define BINEX_START 0x01
#define BINEX_CHAR 0x02
#define BINEX_INVALID 0x03

/******************************************************************************/

static uint8_t *receive_buffer;
static size_t receive_buffer_size;
static uint16_t rxpack_size;
static uint8_t rxstate;
static uint8_t flag_prev_rx_esc;

static uint16_t txpack_size;
static uint8_t *txbuff;
static uint8_t txstate;
static uint8_t flag_prev_tx_esc;

#ifdef BINEX_CHECK_CRC
static uint16_t tx_crc16;
#endif

/******************************************************************************/

static uint8_t char_rx(uint8_t c)
{
  if (flag_prev_rx_esc)
  {
    // Если предыдущий символ является esc-символом
    // Сбрасываем флаг, так как
    // оба сценария развития событий
    // предусматирвают сброс этого флага

    flag_prev_rx_esc = 0;

    // Если пришла экранированная последовательность
    if ((c == BINEX_ESC_SYMBOL) || (c == BINEX_START_SYMBOL))
      return BINEX_CHAR;

    // Если предыдущее условие не выполнилось,
    // то получили некорректную последовательность
    return BINEX_INVALID;
  }

  // Если попали сюда, то это означает, что
  // прошлый раз был принят не esc-символ
  if (c == BINEX_ESC_SYMBOL)
  {
    // Если приняли esc-символ,
    // то устанавливаем соответствующий флаг
    flag_prev_rx_esc = 1;

    // Просто выходим
    return BINEX_ESCAPE;
  }

  // Если пришел стартовый символ
  if (c == BINEX_START_SYMBOL)
    return BINEX_START;

  // Если дошли сюда, то это означает, что приняли
  // обычный символ, возвращаем его
  return BINEX_CHAR;
}

static int char_tx(uint8_t c)
{
  // Если экранирование esc-символа
  if (flag_prev_tx_esc)
  {
    if (binex_tx_callback(c))
    {
      // На предыдущем шаге отправили esc-символ,
      // сейчас сам символ, поэтому возвращаем 1
      flag_prev_tx_esc = 0;
      return 1;
    }
    return 0;
  }

  if ((c == BINEX_ESC_SYMBOL) || (c == BINEX_START_SYMBOL))
  {
    if (binex_tx_callback(BINEX_ESC_SYMBOL))
    {
      flag_prev_tx_esc = 1;
    }

    // Либо ничего не отправили, либо отправили esc-символ,
    // поэтому возвращаем 0
    return 0;
  }

  if (binex_tx_callback(c))
    return 1;

  return 0;
}

/******************************************************************************/

void binex_receiver_begin(uint8_t *buff, size_t buff_size)
{
  receive_buffer = buff;
  receive_buffer_size = buff_size;
  rxstate = 0;
  flag_prev_rx_esc = 0;
  rxpack_size = 0;
}

BinexRxStatus_t binex_receiver(int16_t c)
{
  static uint16_t tmp;
  uint8_t r;

  if (c < 0)
    return BINEX_PACK_NOT_RX;

  switch (rxstate)
  {
  case 0:
    // Начало приема пакета
    // Ожидается символ начала пакета
    if (char_rx(c) == BINEX_START)
    {
      rxstate = 1;
    }
    break;
  //////////////////////////////////////
  case 1:
    /// Прием 1го байта заголовка пакета ///

    r = char_rx((uint8_t)c);
    if (r == BINEX_CHAR)
    {
      // приняли символ
      rxpack_size = ((uint8_t)c);
      rxstate = 2;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      // Если приняли неожиданное начало пакета
      // либо некорректную esc-последовательность
      // переходим в состояние приема старта пакета
      // и возвращаем ошибку
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 2:
    /// Прием 2го байта заголовка пакета ///

    r = char_rx((uint8_t)c);
    if (r == BINEX_CHAR)
    {
      rxpack_size |= (((uint8_t)c) << 8);
      tmp = 0;
      rxstate = 3;

      if (rxpack_size > receive_buffer_size)
      {
        // Пакет слишком большой.
        // Это могло произойти по 2м причинам:
        // 1. пакет действительно слишком большой
        // 2. возникла ошибка при приеме размера пакета
        rxstate = 0;
        return BINEX_PACK_BROKEN;
      }
      else if (rxpack_size == 0) // Пустой пакет
      {
#ifdef BINEX_CHECK_CRC
        // Состояние получения CRC16
        rxstate = 4;
#else
        // Если crc не проверяем, то
        // возвращаемся в состояние приема
        // начала пакета, и возвращаем информацию о том,
        // что пакет был принят
        rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 3:
    /// Прием тела пакета ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      // приняли символ
      receive_buffer[tmp++] = (uint8_t)c;
      if (tmp == rxpack_size) // приняли весь пакет
      {
#ifdef BINEX_CHECK_CRC
        // Если crc проверяем
        rxstate = 4; // прием и проверка CRC
#else
        // если crc не проверяем
        rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
    //////////////////////////////////////
#ifdef BINEX_CHECK_CRC
  case 4:
    /// Получение 1го байта crc ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      tmp = ((uint8_t)c);
      rxstate = 5;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 5:
    /// Получение 2го байта crc ///

    r = char_rx((uint8_t)c);

    if (r == BINEX_CHAR)
    {
      tmp |= ((uint8_t)c) << 8;
      rxstate = 0;

      // Вычисление crc для поля длины пакета
      // и поля полезных данных
      uint16_t crc = Crc16StartValue();
      crc = Crc16((uint8_t *)((void *)(&rxpack_size)), 2, crc);
      crc = Crc16(receive_buffer, rxpack_size, crc);

      if (crc == tmp)
        return BINEX_PACK_RX;
      else
        return BINEX_PACK_BROKEN;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
#endif
  }

  return BINEX_PACK_NOT_RX;
}

uint16_t binex_get_rxpack_len(void)
{
  return rxpack_size;
}

void binex_transmitter_init(void *buff, uint16_t size)
{
  txbuff = (uint8_t *)buff;
  txpack_size = size;
  txstate = 0;
  flag_prev_tx_esc = 0;
#ifdef BINEX_CHECK_CRC
  tx_crc16 = Crc16StartValue();
  tx_crc16 = Crc16((uint8_t *)((void *)(&txpack_size)), 2, tx_crc16);
  tx_crc16 = Crc16(txbuff, size, tx_crc16);
#endif
}

BinexTxStatus_t binex_transmit(void)
{
  static uint16_t tmp;

  for (;;)
  {
    switch (txstate)
    {
    case 0:
      /// Начало процесса передачи ///
      if (binex_tx_callback((uint8_t)BINEX_START_SYMBOL))
      {
        tmp = txpack_size;
        txstate = 1;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 1:
      /// Передача 1го байта длины пакета ///
      if (char_tx((uint8_t)tmp))
      {
        tmp >>= 8;
        txstate = 2;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 2:
      /// Передача 2го байта длины пакета ///
      if (char_tx((uint8_t)tmp))
      {
        tmp = 0;
        txstate = 3;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 3:
      /// Передача тела пакета ///
      // Если все передали
      if (tmp >= txpack_size)
      {
#ifdef BINEX_CHECK_CRC
        txstate = 4;
        break;
#else
        txstate = 6;
        return BINEX_PACK_TX;
#endif
      }

      if (char_tx(txbuff[tmp]))
        tmp++;
      else
        return BINEX_PACK_NOT_TX;
      break;
      /////////////////////////////////////////////
#ifdef BINEX_CHECK_CRC
    case 4:
      /// Передача 1го байта CRC ///
      if (char_tx((uint8_t)tx_crc16))
      {
        tx_crc16 >>= 8;
        txstate = 5;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 5:
      /// Передача 2го байта CRC ///
      if (char_tx((uint8_t)tx_crc16))
      {
        txstate = 6;
        return BINEX_PACK_TX;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
#endif
    /////////////////////////////////////////////
    default:
      return BINEX_PACK_TX;
    }
  }
}
