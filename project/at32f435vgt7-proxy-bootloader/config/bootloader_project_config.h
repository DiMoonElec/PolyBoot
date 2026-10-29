#ifndef __BOOTLOADER_PROJECT_CONFIG_H__
#define __BOOTLOADER_PROJECT_CONFIG_H__

#define BOOTLOADER_UART_BAUD 115200

/* Опциональные команды PolyBoot Proxy V1. */
#define BOOTLOADER_USE_PROXY
#define BOOTLOADER_RESPONSE_DELAY_MS 20
#define BOOTLOADER_TIMEOUT_MS 10000

/* Отвечать ОК на команду запуска приложения, оставаясь в загрузчике. */
//#define BOOTLOADER_DISABLE_APP_RUN_CMD

/* Первые 16 Кбайт — загрузчик; последний сектор 2 Кбайт — настройки. */
#define BOOTLOADER_APP_BEGIN 0x08004000UL
#define BOOTLOADER_APP_LENGTH (1006UL * 1024UL)
#define BOOTLOADER_DEVICE_ID_STRING "at32f435vgt7-proxy-bootloader"

#endif
