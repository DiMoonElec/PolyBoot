#ifndef __BOOTLOADER_PROXY_H__
#define __BOOTLOADER_PROXY_H__

#include <stdint.h>

/* Инициализация только состояния модуля; аппаратный INIT выполняется по A0. */
void BootloaderProxyInit(void);
void BootloaderProxyDeinit(void);
uint8_t BootloaderProxyIsCommand(uint8_t command);

/* Вызывается после активации, когда ответ модуля не передаётся ПК.
   При необходимости модуль копирует запрос в собственный буфер. */
void BootloaderProxyRequest(const uint8_t *request, uint16_t length);
void BootloaderProxyProcess(void);

/* 0 — ответа нет. Иначе payload принадлежит модулю и остаётся неизменным
   до BootloaderProxyResponseSent(), вызываемой после передачи в поток ПК. */
uint16_t BootloaderProxyGetResponse(uint8_t **payload);
void BootloaderProxyResponseSent(void);

#endif
