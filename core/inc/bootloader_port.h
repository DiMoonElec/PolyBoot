#ifndef __BOOTLOADER_PORT_H__
#define __BOOTLOADER_PORT_H__

#include <stdint.h>

/*
  Деинициализация всей задействованной периферии
*/
void port_deinit_all(void);

/*
  Выпонить запуск основного приложения
*/
void port_application_run(void);

/*
  Возвращает состояние перемычки BOOT
  Возвращает:
    1 - активное состояние перемычки (Bootloader активен)
    0 - пассивное состояние перемычки (Bootloader не активен)
*/
int port_boot_jumper_is_active(void);

/*
  Отправить символ в последовательный интерфейс связи
  Возвращает:
    с - отправлено успешно
    -1 - буфер передатчика полон 
*/
int16_t port_serial_putc(uint8_t c);

/*
  Завершена ли передача последовательным интерфейсом
  Возвращает:
    1 - завершено
    0 - не завершено
*/
int port_serial_transfer_completed(void);

/*
  Прочитать символ из буфера приемника
  Возвращает:
    0-255 - символ, полученный по интерфейсу связи
    -1 - буфер приемника пуст
*/
int16_t port_serial_getc(void);

/*
  Проверить, очищен ли данный сектор
*/
uint8_t port_sector_isclear(uint32_t sector);

/*
  Выполнить очистку сектора
*/
uint8_t port_sector_erase(uint32_t adr);

/*
  Выполнить запись чанка во flash-память МК 
*/
uint8_t port_write_chunk(uint8_t *chunk, uint32_t address, uint16_t len);

#endif