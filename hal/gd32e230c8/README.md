# HAL-слой для микроконтроллера GD32E230C8

Данный HAL содержит следующее:
- Реализации функций работы с flash-памятью
- HAL-часть конфигурации Bootloader-а
- Бибоиотеки SPL для микроконтроллера
- Board-независимый код CMSIS. Файлы system_gd32e23x.c, gd32e23x.h и gd32e23x_libopt.h вынесены из CMSIS в board-зависимую часть (в project).

Здесь реализованы следующие API-вызовы ядра:

- ```void port_application_run(void)```
- ```uint8_t port_sector_isclear(uint32_t sector)```
- ```uint8_t port_sector_erase(uint32_t page_addr)```
- ```uint8_t port_write_chunk(uint8_t *chunk, uint32_t address, uint16_t len)```
