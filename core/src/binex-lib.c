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

#include <string.h>
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

static uint8_t char_rx(Binex_t *binex, uint8_t c)
{
  if (binex->flag_prev_rx_esc)
  {
    // Если предыдущий символ является esc-символом
    // Сбрасываем флаг, так как
    // оба сценария развития событий
    // предусматирвают сброс этого флага

    binex->flag_prev_rx_esc = 0;

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
    binex->flag_prev_rx_esc = 1;

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

static int char_tx(Binex_t *binex, uint8_t c)
{
  // Если экранирование esc-символа
  if (binex->flag_prev_tx_esc)
  {
    if (binex->tx_callback(c))
    {
      // На предыдущем шаге отправили esc-символ,
      // сейчас сам символ, поэтому возвращаем 1
      binex->flag_prev_tx_esc = 0;
      return 1;
    }
    return 0;
  }

  if ((c == BINEX_ESC_SYMBOL) || (c == BINEX_START_SYMBOL))
  {
    if (binex->tx_callback(BINEX_ESC_SYMBOL))
    {
      binex->flag_prev_tx_esc = 1;
    }

    // Либо ничего не отправили, либо отправили esc-символ,
    // поэтому возвращаем 0
    return 0;
  }

  if (binex->tx_callback(c))
    return 1;

  return 0;
}

/******************************************************************************/

void binex_init(Binex_t *binex, int (*tx_callback)(uint8_t c))
{
  memset(binex, 0, sizeof(*binex));
  binex->tx_callback = tx_callback;
  binex->txstate = 6; // Передача не запущена.
}

void binex_receiver_begin(Binex_t *binex, uint8_t *buff, size_t buff_size)
{
  binex->receive_buffer = buff;
  binex->receive_buffer_size = buff_size;
  binex->rxstate = 0;
  binex->flag_prev_rx_esc = 0;
  binex->rxpack_size = 0;
  binex->rx_tmp = 0;
}

BinexRxStatus_t binex_receiver(Binex_t *binex, int16_t c)
{
  uint8_t r;

  if (c < 0)
    return BINEX_PACK_NOT_RX;

  switch (binex->rxstate)
  {
  case 0:
    // Начало приема пакета
    // Ожидается символ начала пакета
    if (char_rx(binex, c) == BINEX_START)
    {
      binex->rxstate = 1;
    }
    break;
  //////////////////////////////////////
  case 1:
    /// Прием 1го байта заголовка пакета ///

    r = char_rx(binex, (uint8_t)c);
    if (r == BINEX_CHAR)
    {
      // приняли символ
      binex->rxpack_size = ((uint8_t)c);
      binex->rxstate = 2;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      // Если приняли неожиданное начало пакета
      // либо некорректную esc-последовательность
      // переходим в состояние приема старта пакета
      // и возвращаем ошибку
      binex->rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 2:
    /// Прием 2го байта заголовка пакета ///

    r = char_rx(binex, (uint8_t)c);
    if (r == BINEX_CHAR)
    {
      binex->rxpack_size |= (((uint8_t)c) << 8);
      binex->rx_tmp = 0;
      binex->rxstate = 3;

      if (binex->rxpack_size > binex->receive_buffer_size)
      {
        // Пакет слишком большой.
        // Это могло произойти по 2м причинам:
        // 1. пакет действительно слишком большой
        // 2. возникла ошибка при приеме размера пакета
        binex->rxstate = 0;
        return BINEX_PACK_BROKEN;
      }
      else if (binex->rxpack_size == 0) // Пустой пакет
      {
#ifdef BINEX_CHECK_CRC
        // Состояние получения CRC16
        binex->rxstate = 4;
#else
        // Если crc не проверяем, то
        // возвращаемся в состояние приема
        // начала пакета, и возвращаем информацию о том,
        // что пакет был принят
        binex->rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      binex->rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 3:
    /// Прием тела пакета ///

    r = char_rx(binex, (uint8_t)c);

    if (r == BINEX_CHAR)
    {
      // приняли символ
      binex->receive_buffer[binex->rx_tmp++] = (uint8_t)c;
      if (binex->rx_tmp == binex->rxpack_size) // приняли весь пакет
      {
#ifdef BINEX_CHECK_CRC
        // Если crc проверяем
        binex->rxstate = 4; // прием и проверка CRC
#else
        // если crc не проверяем
        binex->rxstate = 0;
        return BINEX_PACK_RX;
#endif
      }
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      binex->rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
    //////////////////////////////////////
#ifdef BINEX_CHECK_CRC
  case 4:
    /// Получение 1го байта crc ///

    r = char_rx(binex, (uint8_t)c);

    if (r == BINEX_CHAR)
    {
      binex->rx_tmp = ((uint8_t)c);
      binex->rxstate = 5;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      binex->rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
  //////////////////////////////////////
  case 5:
    /// Получение 2го байта crc ///

    r = char_rx(binex, (uint8_t)c);

    if (r == BINEX_CHAR)
    {
      binex->rx_tmp |= ((uint8_t)c) << 8;
      binex->rxstate = 0;

      // Вычисление crc для поля длины пакета
      // и поля полезных данных
      uint16_t crc = Crc16StartValue();
      crc = Crc16((uint8_t *)((void *)(&binex->rxpack_size)), 2, crc);
      crc = Crc16(binex->receive_buffer, binex->rxpack_size, crc);

      if (crc == binex->rx_tmp)
        return BINEX_PACK_RX;
      else
        return BINEX_PACK_BROKEN;
    }
    else if ((r == BINEX_START) || (r == BINEX_INVALID))
    {
      binex->rxstate = 0;
      return BINEX_PACK_BROKEN;
    }
    break;
#endif
  }

  return BINEX_PACK_NOT_RX;
}

uint16_t binex_get_rxpack_len(const Binex_t *binex)
{
  return binex->rxpack_size;
}

void binex_transmitter_init(Binex_t *binex, void *buff, uint16_t size)
{
  binex->txbuff = (uint8_t *)buff;
  binex->txpack_size = size;
  binex->txstate = 0;
  binex->tx_tmp = 0;
  binex->flag_prev_tx_esc = 0;
#ifdef BINEX_CHECK_CRC
  binex->tx_crc16 = Crc16StartValue();
  binex->tx_crc16 = Crc16((uint8_t *)((void *)(&binex->txpack_size)), 2, binex->tx_crc16);
  binex->tx_crc16 = Crc16(binex->txbuff, size, binex->tx_crc16);
#endif
}

BinexTxStatus_t binex_transmit(Binex_t *binex)
{
  for (;;)
  {
    switch (binex->txstate)
    {
    case 0:
      /// Начало процесса передачи ///
      if (binex->tx_callback((uint8_t)BINEX_START_SYMBOL))
      {
        binex->tx_tmp = binex->txpack_size;
        binex->txstate = 1;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 1:
      /// Передача 1го байта длины пакета ///
      if (char_tx(binex, (uint8_t)binex->tx_tmp))
      {
        binex->tx_tmp >>= 8;
        binex->txstate = 2;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 2:
      /// Передача 2го байта длины пакета ///
      if (char_tx(binex, (uint8_t)binex->tx_tmp))
      {
        binex->tx_tmp = 0;
        binex->txstate = 3;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 3:
      /// Передача тела пакета ///
      // Если все передали
      if (binex->tx_tmp >= binex->txpack_size)
      {
#ifdef BINEX_CHECK_CRC
        binex->txstate = 4;
        break;
#else
        binex->txstate = 6;
        return BINEX_PACK_TX;
#endif
      }

      if (char_tx(binex, binex->txbuff[binex->tx_tmp]))
        binex->tx_tmp++;
      else
        return BINEX_PACK_NOT_TX;
      break;
      /////////////////////////////////////////////
#ifdef BINEX_CHECK_CRC
    case 4:
      /// Передача 1го байта CRC ///
      if (char_tx(binex, (uint8_t)binex->tx_crc16))
      {
        binex->tx_crc16 >>= 8;
        binex->txstate = 5;
      }
      else
        return BINEX_PACK_NOT_TX;
      break;
    /////////////////////////////////////////////
    case 5:
      /// Передача 2го байта CRC ///
      if (char_tx(binex, (uint8_t)binex->tx_crc16))
      {
        binex->txstate = 6;
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
