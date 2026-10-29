#ifndef __BOOTLOADER_PROXY_PORT_H__
#define __BOOTLOADER_PROXY_PORT_H__

#include <stdint.h>

typedef enum
{
  PROXY_PORT_BUSY,
  PROXY_PORT_DONE,
  PROXY_PORT_ERROR
} ProxyPortResult_t;

typedef enum
{
  PROXY_LIST_DONE,
  PROXY_LIST_MORE,
  PROXY_LIST_ERROR
} ProxyListResult_t;

/* begin начинает операцию, poll выполняет короткий неблокирующий шаг.
   Итоговый результат poll сохраняется до следующего begin.
   Повторная инициализация освобождает прежний канал и обновляет список. */
void port_proxy_init_begin(void);
ProxyPortResult_t port_proxy_init_poll(void);

/* offset — индекс в стабильном списке, а не ID. Записать не более capacity
   ID и вернуть их количество через count. При MORE count должен быть > 0.
   Допускаются все ID 0..255; список может содержать 256 элементов.
   Обнаружение устройств выполняется при init, эта функция не ожидает ответа. */
ProxyListResult_t port_proxy_get_slave_list(uint16_t offset, uint8_t *ids,
                                          uint16_t capacity, uint16_t *count);

/* OPEN сам освобождает прежнего слейва, очищает FIFO и выдерживает время
   переключения. DONE означает готовность нового слейва к обмену.
   При ERROR все слейвы должны быть отключены от общей линии. */
void port_proxy_open_begin(uint8_t slave_id);
ProxyPortResult_t port_proxy_open_poll(void);

/* CLOSE идемпотентен, в том числе до INIT. Не сбрасывает ядро слейва. */
void port_proxy_close_begin(void);
ProxyPortResult_t port_proxy_close_poll(void);

/* Освободить всё, что настроил порт. Допустим вызов до первого INIT. */
void port_proxy_deinit(void);

/* Поток обычных Binex-пакетов между мастером и выбранным слейвом.
   putc: c либо -1 при заполнении; getc: 0..255 либо -1 при отсутствии данных.
   completed учитывает физическое завершение передачи последнего байта. */
int16_t port_proxy_serial_putc(uint8_t c);
int16_t port_proxy_serial_getc(void);
int port_proxy_serial_transfer_completed(void);

/* Буферизация — ответственность проекта. Ядро пересылает ВСЕ ответы,
   включая каждое событие стирания, и не регулирует скорость слейва.
   Во время отправки ответа ПК и выполнения обычных команд ядра приём
   из FIFO может приостанавливаться. Проект обязан рассчитать буфер на
   максимальный поток либо обеспечить собственное управление скоростью.
   Молчаливая потеря байтов при переполнении недопустима. */

#endif
