#include "bootloader.h"
#include "bootloader_port.h"
#include "port_hardware.h"
#include "serial_port.h"
#include "systick.h"

int16_t port_serial_putc(uint8_t c) 
{ 
  return SerialPortPutc(c); 
}

int port_serial_transfer_completed(void) 
{ 
  return SerialPortTransferCompleted(); 
}

int16_t port_serial_getc(void) 
{ 
  return SerialPortGetc(); 
}

void port_deinit_all(void)
{
  SysTick_Deinit();
  hw_deinit();
}

int port_boot_jumper_is_active(void)
{
  return 0;
}

void main(void)
{
  SysTick_Init();
  hw_init();

  SerialPortInit();

  InitBootloader();
  
  for(;;)
  {
    ProcessBootloader();
  }
}


#if 0

#include "at32f413.h"

#include "bootloader.h"
#include "status_led.h"

#include "crc32.h"
#include "crc16.h"


#define RELAY8_PIN    GPIO_PINS_13
#define RELAY7_PIN    GPIO_PINS_14
#define RELAY6_PIN    GPIO_PINS_15
#define RELAY5_PIN    GPIO_PINS_6
#define RELAY4_PIN    GPIO_PINS_7
#define RELAY3_PIN    GPIO_PINS_8
#define RELAY2_PIN    GPIO_PINS_9
#define RELAY1_PIN    GPIO_PINS_8

static void __gpio_safe_init(void)
{
  gpio_init_type gpio_init_struct;
  gpio_default_para_init(&gpio_init_struct);
  
  gpio_bits_reset(GPIOC, RELAY5_PIN | RELAY4_PIN | RELAY3_PIN | RELAY2_PIN);
  gpio_bits_reset(GPIOB, RELAY8_PIN | RELAY7_PIN | RELAY6_PIN);
  gpio_bits_reset(GPIOA, RELAY1_PIN);

  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = RELAY1_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOA, &gpio_init_struct);

  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = RELAY5_PIN | RELAY4_PIN | RELAY3_PIN | RELAY2_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOC, &gpio_init_struct);

  gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
  gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
  gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
  gpio_init_struct.gpio_pins = RELAY8_PIN | RELAY7_PIN | RELAY6_PIN;
  gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
  gpio_init(GPIOB, &gpio_init_struct);

}

void main()
{
  gpio_pin_remap_config(SWJTAG_GMUX_010, TRUE);
  __gpio_safe_init();
  
  crc32_table();    // Инициализация стаблицы crc32
  Crc16InitTable(); // Инициализация стаблицы crc16
  
  InitBootloader(); // Инициализация загрузчика
  StatusLed_Init(); // Инициализация светодиода
  
  StatusLed_ModeBlink(125, 125);
  
  for(;;)
  {
    ProcessBootloader();
    StatusLed_Process();
  }
}


#endif