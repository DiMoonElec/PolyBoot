#include <string.h>
#include "bootloader.h"
#include "bootloader_port.h"
#include "binex-lib.h"
#include "utils.h"
#include "crc16.h"
#include "monocypher.h"
#include "systick.h"
#include "bootloader_hal_config.h"
#include "bootloader_project_config.h"

/******************************************************************************/

#include "private_keys.inc"

/******************************************************************************/

#define BUFFER_EXCH_SIZE 256

#define CHUNK_DATA_SIZE 128
#define MAC_SIZE 16

/******************************************************************************/

#define CMD_ACTIVATE 0x70
#define CMD_BEGIN 0x71
#define CMD_SEND 0x72
#define CMD_WRITE 0x73
#define CMD_END 0x74
#define CMD_CHECK_CRC 0x75
#define CMD_APP_RUN 0x76
#define CMD_ERASE_USER_DATA 0x78

/******************************************************************************/

enum
{
  STATE_MAIN = 0,
  STATE_RX_WAIT,
  STATE_BEGIN,
  STATE_FLASH_CLEAR,
  STATE_SEND_EVENT_FLASH_CLEAR,
  STATE_SEND_EVENT_FLASH_CLEAR_1,

  STATE_USER_DATA_CLEAR_BEGIN,
  STATE_USER_DATA_CLEAR,
  STATE_SEND_EVENT_USER_DATA_CLEAR,
  STATE_SEND_EVENT_USER_DATA_CLEAR_1,

  STATE_APP_RUN,
  STATE_APP_RUN_1,
  STATE_APP_RUN_2,

  STATE_SEND_RESP,
  STATE_SEND_RESP_1,

  NUM_STATES
};

#pragma pack(push, 1)

struct fw_chunk_s
{
  uint32_t address;  // AAD
  uint8_t len;       // AAD
  uint8_t nonce[24]; // nonce (IETF)
  uint8_t ciphertext[CHUNK_DATA_SIZE];
  uint8_t tag[16]; // Poly1305
};

#pragma pack(pop)

/******************************************************************************/

static uint8_t state, _state;
static uint8_t flag_activated;
static uint8_t flag_firmware_valid;
static uint8_t flag_begin;
static uint32_t adr_counter;
static uint32_t timer;

#ifdef BOOTLOADER_TIMEOUT_MS
static uint32_t boot_timer;
#endif

static uint8_t buffer_exch[BUFFER_EXCH_SIZE];

static uint8_t Data[CHUNK_DATA_SIZE]; // Буфер, в котором содержится расшифрованный кусок прошивки

static uint8_t DataLen;        // Размер полезных данных в Data
static uint32_t DataAddress;   // Смещение во flash, начиная с которого необходимо записать Data
static uint8_t flag_DataIsSet; // Флаг наличия полезных данных в буфере Data

/* Строковая константа активации загрузчика */
static const uint8_t activate_data[] = {'A', 'C', 'T', 'I', 'V', 'A', 'T', 'E'};

/* Ожидаемая строка устройства */
static const uint8_t expected_device_id[CHUNK_DATA_SIZE] = BOOTLOADER_DEVICE_ID_STRING;

/******************************************************************************/

/*
  Сравнение двух участков памяти
  Возвращает:
    1 - участки хранят одинаковые данные
    0 - данные различаются
*/
static uint8_t __memcompare(const uint8_t *dest, const uint8_t *source, int len)
{
  for (int i = 0; i < len; i++)
  {
    if (dest[i] != source[i])
      return 0;
  }

  return 1;
}

static uint8_t __check_activation_signature(const uint8_t *sgnt)
{
  for (int i = 0; i < sizeof(activate_data); i++)
  {
    if (sgnt[i] != activate_data[i])
      return 1;
  }

  return 0;
}

static uint8_t __app_poly1305_check(void)
{
  const uint8_t *flash_begin = (const uint8_t *)BOOTLOADER_APP_BEGIN;
  uint32_t firmware_size = BOOTLOADER_APP_LENGTH - MAC_SIZE;

  const uint8_t *flash_mac =
      (const uint8_t *)(BOOTLOADER_APP_BEGIN + firmware_size);

  uint8_t calc_mac[MAC_SIZE];

  // 1. Считаем Poly1305 MAC по прошивке
  crypto_poly1305(calc_mac, flash_begin, firmware_size, IntegrityKey);

  // 2. Сравниваем с сохранённым
  if (crypto_verify16(calc_mac, flash_mac) != 0)
    return 1; // MAC неверен

  return 0; // OK
}

static int __decrypt_and_verify_chunk(
    const uint8_t *key,
    const struct fw_chunk_s *chunk,
    uint8_t plaintext[CHUNK_DATA_SIZE])
{
  /* AAD = address || len */
  uint8_t aad[5];

  /* address */
  aad[0] = (uint8_t)(chunk->address >> 0);
  aad[1] = (uint8_t)(chunk->address >> 8);
  aad[2] = (uint8_t)(chunk->address >> 16);
  aad[3] = (uint8_t)(chunk->address >> 24);

  /* len */
  aad[4] = chunk->len;

  /* Проверка и расшифровка, используется XChaCha20-Poly1305*/
  int ret = crypto_aead_unlock(
      plaintext,        // out: расшифрованные данные
      chunk->tag,       // MAC
      key,              // ключ
      chunk->nonce,     // nonce (24 байт)
      aad, sizeof(aad), // AAD
      chunk->ciphertext,
      CHUNK_DATA_SIZE);

  /* crypto_aead_unlock():
     0  -> OK
    -1  -> MAC не совпал
  */
  if (ret != 0)
  {
    crypto_wipe(plaintext, CHUNK_DATA_SIZE);
    return -1;
  }

  return 0;
}

/*
  Проверка идентификационного чанка устройства
  Возвращает:
    1 - проверка не прошла
    0 - проверка прошла, все ОК
*/
static uint8_t __check_identity_chunk(void)
{
  const struct fw_chunk_s *chunk =
      (const struct fw_chunk_s *)(buffer_exch + 1);

  /*
    Логическая проверка
      len всегда должно быть равно CHUNK_DATA_SIZE
    address - в данном конексте хранит общий
      ожидаемый размер приложения,
      значение должно быть равно BOOTLOADER_APP_LENGTH
  */
  if ((chunk->len != CHUNK_DATA_SIZE) ||
      (chunk->address != BOOTLOADER_APP_LENGTH))
  {
    return 1;
  }

  /* Расшифровка + аутентификация */
  if (__decrypt_and_verify_chunk(
          EncryptionKey,
          chunk,
          Data) != 0)
  {
    return 1; // MAC / decrypt error
  }

  /* Сравнение идентификационной строки */
  if (__memcompare(Data, expected_device_id, CHUNK_DATA_SIZE) == 0)
  {
    return 1; // не тот девайс
  }

  /* Всё ОК */
  return 0;
}

static uint8_t __decrypt_chunk(void)
{
  flag_DataIsSet = 0;

  const struct fw_chunk_s *chunk =
      (const struct fw_chunk_s *)(buffer_exch + 1);

  /* Проверка len заранее (логическая, не крипто) */
  if (chunk->len == 0 || chunk->len > CHUNK_DATA_SIZE)
    return 1;

  /* Расшифровка + аутентификация */
  if (__decrypt_and_verify_chunk(
          EncryptionKey,
          chunk,
          Data) != 0)
  {
    return 1; // MAC не сошёлся
  }

  /* Если мы здесь — данные подлинные */
  DataLen = chunk->len;
  DataAddress = chunk->address;

  /* Проверяем корректность DataLen и DataAddress */
  if (DataLen > CHUNK_DATA_SIZE)
  {
    return 1;
  }

  if ((DataAddress < BOOTLOADER_APP_BEGIN) ||
      ((DataAddress + DataLen) > (BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH)))
  {
    return 1;
  }

  /* Если попали сюда, то все ОК */
  flag_DataIsSet = 1;

  return 0;
}

static void __app_run(void)
{
  port_deinit_all();
  port_application_run();
}

static void __parsecmd(void)
{
  uint16_t len = binex_get_rxpack_len();
  if (len == 0)
  {
    state = STATE_MAIN;
    return;
  }

  switch (buffer_exch[0])
  {
  /////////////////////////////////////////
  case CMD_ACTIVATE:
    if (len != 9)
    {
      state = STATE_MAIN;
      break;
    }

    // Проверяем сигнатуру
    if (__check_activation_signature(buffer_exch + 1))
    {
      state = STATE_MAIN;
      break;
    }

    // Если попали сюда, то все ОК
    flag_activated = 1;

    // Заполняем буфер ответа
    buffer_exch[0] = CMD_ACTIVATE;
    buffer_exch[1] = 0x00;

    // Инициализируем передатчик
    binex_transmitter_init(buffer_exch, 2);
    state = STATE_SEND_RESP;
    break;
  /////////////////////////////////////////
  case CMD_BEGIN:
    /*
      Команда BEGIN, в качестве быстрой проверки перед destructive-op,
      принимает идентификационный чанк файла обновления, и выполняет его валидацию.
      Если валидация завершилась успешно, то можно сделать косвенный вывод,
      что пользователь не перепутал файл обновления, и
      это файл обновления именно для данного изделия, и дальнейший процесс
      приема прошивки скорее всего завершится успешно.
      Это является защитой от невнимательности и "кривых рук", 
      позволяющая не оставить пользователя с окирпиченным девайсом 
      в самый неподходящий момент времени.
    */
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    if (len != (1 + sizeof(struct fw_chunk_s)))
    {
      state = STATE_MAIN;
      break;
    }

    if (__check_identity_chunk() != 0)
    {
      buffer_exch[0] = CMD_BEGIN;
      buffer_exch[1] = 0x02;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }

    state = STATE_BEGIN;
    break;
    /////////////////////////////////////////
#ifdef BOOTLOADER_USE_USER_DATA
  case CMD_ERASE_USER_DATA:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    state = STATE_USER_DATA_CLEAR_BEGIN;
    break;
#endif
  /////////////////////////////////////////
  case CMD_SEND:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    if (len != (1 + sizeof(struct fw_chunk_s)))
    {
      state = STATE_MAIN;
      break;
    }

    // Заполняем буфер ответа
    buffer_exch[0] = CMD_SEND;

    if (__decrypt_chunk() != 0)
      buffer_exch[1] = 0x01; // ошибка расшифровки
    else
      buffer_exch[1] = 0x00; // иначе ОК

    binex_transmitter_init(buffer_exch, 2);
    state = STATE_SEND_RESP;
    break;
  /////////////////////////////////////////
  case CMD_WRITE:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    buffer_exch[0] = CMD_WRITE;

    if ((flag_begin == 0) || (flag_DataIsSet == 0))
    {
      buffer_exch[1] = 0x02;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }

    if (__memcompare((const uint8_t *)DataAddress, Data, DataLen))
    {
      // Если участки памяти совпадают,
      // то просто возвращаем ОК
      // без повторной записи данных
      buffer_exch[1] = 0x00; // ОК
    }
    else if (port_write_chunk(Data, DataAddress, DataLen) != 0)
    {
      buffer_exch[1] = 0x01; // ошибка записи
    }
    else
    {
      buffer_exch[1] = 0x00; // иначе ОК
    }

    binex_transmitter_init(buffer_exch, 2);
    state = STATE_SEND_RESP;
    break;
  /////////////////////////////////////////
  case CMD_END:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    flag_begin = 0;
    buffer_exch[0] = CMD_END;
    buffer_exch[1] = 0x00;
    binex_transmitter_init(buffer_exch, 2);
    state = STATE_SEND_RESP;
    break;
  /////////////////////////////////////////
  case CMD_CHECK_CRC:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    buffer_exch[0] = CMD_CHECK_CRC;

    if (flag_begin)
    {
      // В этом состоянии нельзя выполнять это действие,
      // сначала надо до конца записать прошивку
      buffer_exch[1] = 0x02;
    }
    else if (__app_poly1305_check() != 0)
    {
      flag_firmware_valid = 0;
      buffer_exch[1] = 0x01; // ошибка расшифровки
    }
    else
    {
      flag_firmware_valid = 1;
      buffer_exch[1] = 0x00; // иначе ОК
    }

    binex_transmitter_init(buffer_exch, 2);
    state = STATE_SEND_RESP;
    break;
  /////////////////////////////////////////
  case CMD_APP_RUN:
    if (flag_activated == 0)
    {
      state = STATE_MAIN;
      break;
    }

    buffer_exch[0] = CMD_APP_RUN;

    if (flag_begin)
    {
      // В этом состоянии нельзя выполнять это действие,
      // сначала надо до конца записать прошивку
      buffer_exch[1] = 0x02;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }
    else if (__app_poly1305_check() != 0) // Если основная прошивка отсутствует, либо повреждена
    {
      flag_firmware_valid = 0;
      buffer_exch[1] = 0x01;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }

    // Отправляем ответ, после чего запускаем прошивку
    buffer_exch[1] = 0x00;
    binex_transmitter_init(buffer_exch, 2);
    state = STATE_APP_RUN;
    break;
  }
}

/******************************************************************************/

void InitBootloader(void)
{
  Crc16Init();

  state = STATE_MAIN;
  _state = state + 1;

  flag_begin = 0;
  flag_DataIsSet = 0;

  flag_activated = 0;

#if !defined(BOOTLOADER_DBG_MODE)
  // Если не ноль, то приложение не прошло
  // проверку целостности
  if (__app_poly1305_check())
    flag_firmware_valid = 0;
  else
    flag_firmware_valid = 1;
#else
  // В отладочной сборке проверку целостности не проводим
  flag_firmware_valid = 1;
#endif

#ifdef BOOTLOADER_TIMEOUT_MS
  boot_timer = SYSTICK_GET_VALUE();
#else
  if (port_boot_jumper_is_active())
    flag_activated = 1;
#endif
}

void ProcessBootloader(void)
{
  uint8_t entry = 0;
  if (_state != state)
  {
    _state = state;
    entry = 1;
  }

  switch (state)
  {
  /*********************************************/
  case STATE_MAIN:
    binex_receiver_begin(buffer_exch, BUFFER_EXCH_SIZE);
    state = STATE_RX_WAIT;
    break;
  /*********************************************/
  case STATE_RX_WAIT:
    if (binex_receiver(port_serial_getc()) == BINEX_PACK_RX)
      __parsecmd();

#ifdef BOOTLOADER_TIMEOUT_MS                                              /* Если Bootloader активируется по тайм-ауту */
    if ((!flag_activated)                                                 // Если Bootloader не был активирован командой
        && (flag_firmware_valid)                                          // и прошивка прошла проверку целостности
        && ((SYSTICK_GET_VALUE() - boot_timer) >= BOOTLOADER_TIMEOUT_MS)) // и истек таймаут
    {
      // Запускаем основную прошивку
      __app_run();
    }
#else /* Иначе, Bootloader активируется по внешнему сигналу */
    // Если загрузчик не переведен в активное состояние
    if ((!flag_activated) && (flag_firmware_valid))
    {
      // Запускаем основную прошивку
      __app_run();
    }
#endif

    break;
  /*********************************************/
  case STATE_BEGIN:
  {
    flag_firmware_valid = 0;
    adr_counter = BOOTLOADER_APP_BEGIN;
    state = STATE_FLASH_CLEAR;
  }
  break;
  /*********************************************/
  case STATE_FLASH_CLEAR:
  {
    // Если очистили всю область
    if (adr_counter >= (BOOTLOADER_APP_BEGIN + BOOTLOADER_APP_LENGTH))
    {
      flag_begin = 1;
      flag_DataIsSet = 0;

      // Возвращаем OK
      buffer_exch[0] = CMD_BEGIN;
      buffer_exch[1] = 0x00;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }

    // Если текущий сектор не очищен,
    // то очищаем его
    if (!port_sector_isclear(adr_counter))
    {
      port_sector_erase(adr_counter);

      // Если ошибка очистки сектора, то выходим с ошибкой
      if (!port_sector_isclear(adr_counter))
      {
        uint32_t block = (adr_counter - BOOTLOADER_APP_BEGIN) / BOOTLOADER_FLASH_SECTOR_SIZE;
        buffer_exch[0] = CMD_BEGIN;
        buffer_exch[1] = 0x01;
        UInt32ToBuff(buffer_exch + 2, block);
        binex_transmitter_init(buffer_exch, 6);
        state = STATE_SEND_RESP;
        break;
      }
    }

    adr_counter += BOOTLOADER_FLASH_SECTOR_SIZE;

    uint32_t block = (adr_counter - BOOTLOADER_APP_BEGIN) / BOOTLOADER_FLASH_SECTOR_SIZE;
    buffer_exch[0] = CMD_BEGIN;
    buffer_exch[1] = 0xFF;
    UInt32ToBuff(buffer_exch + 2, BOOTLOADER_APP_LENGTH / BOOTLOADER_FLASH_SECTOR_SIZE);
    UInt32ToBuff(buffer_exch + 6, block);
    binex_transmitter_init(buffer_exch, 10);
    state = STATE_SEND_EVENT_FLASH_CLEAR;
  }
  break;
    /*********************************************/
  case STATE_SEND_EVENT_FLASH_CLEAR:
    if (binex_transmit() == BINEX_PACK_TX)
      state = STATE_SEND_EVENT_FLASH_CLEAR_1;
    break;
  /*********************************************/
  case STATE_SEND_EVENT_FLASH_CLEAR_1:
    if (port_serial_transfer_completed())
      state = STATE_FLASH_CLEAR;
    break;
    /*********************************************/
#ifdef BOOTLOADER_USE_USER_DATA
  case STATE_USER_DATA_CLEAR_BEGIN:
  {
    adr_counter = USER_DATA_BEGIN;
    state = STATE_USER_DATA_CLEAR;
  }
  break;
  /*********************************************/
  case STATE_USER_DATA_CLEAR:
  {
    // Если очистили всю область
    if (adr_counter >= (USER_DATA_BEGIN + USER_DATA_LENGHT))
    {
      flag_begin = 1;

      // Возвращаем OK
      buffer_exch[0] = CMD_ERASE_USER_DATA;
      buffer_exch[1] = 0x00;
      binex_transmitter_init(buffer_exch, 2);
      state = STATE_SEND_RESP;
      break;
    }

    // Если текущий сектор не очищен,
    // то очищаем его
    if (!port_sector_isclear(adr_counter))
    {
      port_sector_erase(adr_counter);

      // Если ошибка очистки сектора, то выходим с ошибкой
      if (!port_sector_isclear(adr_counter))
      {
        uint32_t block = (adr_counter - USER_DATA_BEGIN) / FLASH_SECTOR_SIZE;
        buffer_exch[0] = CMD_ERASE_USER_DATA;
        buffer_exch[1] = 0x01;
        UInt32ToBuff(buffer_exch + 2, block);
        binex_transmitter_init(buffer_exch, 6);
        state = STATE_SEND_RESP;
        break;
      }
    }

    adr_counter += FLASH_SECTOR_SIZE;

    uint32_t block = (adr_counter - USER_DATA_BEGIN) / FLASH_SECTOR_SIZE;
    buffer_exch[0] = CMD_ERASE_USER_DATA;
    buffer_exch[1] = 0xFF;
    UInt32ToBuff(buffer_exch + 2, USER_DATA_LENGHT / FLASH_SECTOR_SIZE);
    UInt32ToBuff(buffer_exch + 6, block);
    binex_transmitter_init(buffer_exch, 10);
    state = STATE_SEND_EVENT_USER_DATA_CLEAR;
  }
  break;
    /*********************************************/
  case STATE_SEND_EVENT_USER_DATA_CLEAR:
    if (binex_transmit() == BINEX_PACK_TX)
      state = STATE_SEND_EVENT_USER_DATA_CLEAR_1;
    break;
  /*********************************************/
  case STATE_SEND_EVENT_USER_DATA_CLEAR_1:
    if (port_serial_transfer_completed())
      state = STATE_USER_DATA_CLEAR;
    break;
#endif
  /*********************************************/
  case STATE_SEND_RESP:
    if (entry)
    {
      timer = SYSTICK_GET_VALUE();
    }

    if ((SYSTICK_GET_VALUE() - timer) >= BOOTLOADER_RESPONSE_DELAY_MS)
    {
      state = STATE_SEND_RESP_1;
    }
    break;
  /*********************************************/
  case STATE_SEND_RESP_1:
    if (binex_transmit() == BINEX_PACK_TX)
      state = STATE_MAIN;
    break;
  /*********************************************/
  case STATE_APP_RUN:
    if (entry)
    {
      timer = SYSTICK_GET_VALUE();
    }

    if ((SYSTICK_GET_VALUE() - timer) >= BOOTLOADER_RESPONSE_DELAY_MS)
    {
      state = STATE_APP_RUN_1;
    }
    break;
  /*********************************************/
  case STATE_APP_RUN_1:
    if (binex_transmit() == BINEX_PACK_TX)
    {
      state = STATE_APP_RUN_2;
    }
    break;
  /*********************************************/
  case STATE_APP_RUN_2:
    if (entry)
    {
      timer = SYSTICK_GET_VALUE();
    }

    // Тайм-аут завершения передачи
    if ((SYSTICK_GET_VALUE() - timer) >= 300)
    {
      __app_run();
    }
    break;
  /*********************************************/
  default:
    state = STATE_MAIN;

    break;
  }
}

int binex_tx_callback(uint8_t c)
{
  int16_t ret = port_serial_putc(c);
  if (ret == c)
    return 1;
  return 0;
}

/******************************************************************************/