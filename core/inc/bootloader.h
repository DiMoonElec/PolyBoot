#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#include <stdint.h>

/*
 * Ранний запуск приложения до InitBootloader() и настройки оборудования загрузчика.
 * Вызывающий код должен оставить оборудование и прерывания готовыми к переходу
 * в приложение: эта функция не выполняет деинициализацию.
 * Всегда проверяет Poly1305 MAC, в том числе при BOOTLOADER_DBG_MODE.
 * Если проверка не прошла, возвращается без изменения состояния загрузчика.
 * При успешной проверке передаёт управление приложению и не возвращается.
 */
void BootloaderTryRunApplication(void);

void InitBootloader(void);
void ProcessBootloader(void);


#endif
