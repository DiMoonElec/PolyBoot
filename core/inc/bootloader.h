#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include <stdint.h>
#include "bootloader_project_config.h"

/*
 * Ранний запуск приложения до InitBootloader() и настройки оборудования загрузчика.
 * Вызывающий код должен оставить оборудование и прерывания готовыми к переходу
 * в приложение: эта функция не выполняет деинициализацию.
 * Всегда проверяет Poly1305 MAC, в том числе при BOOTLOADER_DBG_MODE.
 * Если проверка не прошла, возвращается без изменения состояния загрузчика.
 * При успешной проверке передаёт управление приложению и не возвращается.
 */
void BootloaderTryRunApplication(void);

#ifdef BOOTLOADER_TIMEOUT_MS
/*
 * Переопределяет тайм-аут ожидания активации в миллисекундах.
 * Вызывать до InitBootloader(); последний вызов задаёт значение.
 * Без вызова используется BOOTLOADER_TIMEOUT_MS. InitBootloader() сохраняет
 * выбранное значение и начинает отсчёт заново.
 * Значение 0 разрешает запуск исправного приложения без ожидания
 * при обработке автомата загрузчика.
 */
void BootloaderSetActivationTimeout(uint32_t timeout_ms);
#endif

void InitBootloader(void);
void ProcessBootloader(void);


#endif
