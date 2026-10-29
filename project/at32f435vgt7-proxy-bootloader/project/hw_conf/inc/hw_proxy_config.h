#ifndef __HW_PROXY_CONFIG_H__
#define __HW_PROXY_CONFIG_H__

#include <stdint.h>

#define PROXY_SLAVE_COUNT 12

void hw_proxy_gpio_config(void);
void hw_proxy_reset_slaves(uint8_t asserted);
void hw_proxy_lines_low(void);
void hw_proxy_uart_pins_config(void);
void hw_proxy_deselect_all(void);
void hw_proxy_select(uint8_t slave_id);
void hw_proxy_uart_config(uint8_t channel);
void hw_proxy_uart_deinit(uint8_t channel);
void hw_proxy_deinit(void);

#endif
