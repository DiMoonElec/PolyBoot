#include "bootloader_project_config.h"

#ifdef BOOTLOADER_USE_PROXY

#include <string.h>
#include "bootloader_proxy.h"
#include "bootloader_proxy_port.h"
#include "binex-lib.h"
#include "systick.h"

#define CMD_PROXY_INIT_SLAVES 0xA0
#define CMD_PROXY_GET_SLAVE_LIST 0xA1
#define CMD_PROXY_OPEN 0xA2
#define CMD_PROXY_CLOSE 0xA3
#define CMD_PROXY_DATA 0xA4

#define PROXY_STATUS_OK 0x00
#define PROXY_STATUS_ERROR 0x01
#define PROXY_STATUS_CONTINUE 0xFF

/* ПК принимает до 512 байт, включая префикс A4. */
#define PROXY_RESPONSE_SIZE 512
#define PROXY_INNER_SIZE (PROXY_RESPONSE_SIZE - 1)
#define PROXY_KEEPALIVE_MS 200UL

enum
{
  PROXY_JOB_NONE,
  PROXY_JOB_INIT,
  PROXY_JOB_LIST,
  PROXY_JOB_OPEN,
  PROXY_JOB_CLOSE
};

static Binex_t slave_protocol;
static uint8_t slave_tx[PROXY_INNER_SIZE];
static uint8_t slave_rx[PROXY_INNER_SIZE];
static uint8_t response[PROXY_RESPONSE_SIZE];
static uint16_t response_length;
static uint16_t list_offset;
static uint32_t keepalive_timer;
static uint8_t job;
static uint8_t initialized;
static uint8_t channel_open;
/* 0 — нет передачи, 1 — формирование Binex, 2 — ожидание UART TDC. */
static uint8_t slave_tx_state;

static int slave_tx_callback(uint8_t c)
{
  return port_proxy_serial_putc(c) == c;
}

static void set_status(uint8_t command, uint8_t status)
{
  response[0] = command;
  response[1] = status;
  response_length = 2;
}

static void reset_channel(void)
{
  channel_open = 0;
  slave_tx_state = 0;
  binex_init(&slave_protocol, slave_tx_callback);
  binex_receiver_begin(&slave_protocol, slave_rx, sizeof(slave_rx));
}

void BootloaderProxyInit(void)
{
  job = PROXY_JOB_NONE;
  initialized = 0;
  response_length = 0;
  reset_channel();
}

void BootloaderProxyDeinit(void)
{
  port_proxy_deinit();
  BootloaderProxyInit();
}

uint8_t BootloaderProxyIsCommand(uint8_t command)
{
  return command >= CMD_PROXY_INIT_SLAVES && command <= CMD_PROXY_DATA;
}

void BootloaderProxyRequest(const uint8_t *request, uint16_t length)
{
  switch (request[0])
  {
  case CMD_PROXY_INIT_SLAVES:
    if (length != 1)
      break;

    reset_channel();
    initialized = 0;
    port_proxy_init_begin();
    keepalive_timer = SYSTICK_GET_VALUE();
    job = PROXY_JOB_INIT;
    break;

  case CMD_PROXY_GET_SLAVE_LIST:
    if (length != 1)
      break;

    if (!initialized || job != PROXY_JOB_NONE)
    {
      set_status(CMD_PROXY_GET_SLAVE_LIST, PROXY_STATUS_ERROR);
      break;
    }

    list_offset = 0;
    job = PROXY_JOB_LIST;
    break;

  case CMD_PROXY_OPEN:
    if (length != 2)
      break;

    if (!initialized || job != PROXY_JOB_NONE)
    {
      set_status(CMD_PROXY_OPEN, PROXY_STATUS_ERROR);
      break;
    }

    reset_channel();
    port_proxy_open_begin(request[1]);
    job = PROXY_JOB_OPEN;
    break;

  case CMD_PROXY_CLOSE:
    if (length != 1)
      break;

    /* Закрытие допустимо и до INIT, и при незавершённой операции. */
    if (job == PROXY_JOB_INIT)
      initialized = 0;
    reset_channel();
    port_proxy_close_begin();
    job = PROXY_JOB_CLOSE;
    break;

  case CMD_PROXY_DATA:
    if (length < 2 || length > PROXY_RESPONSE_SIZE)
      break;

    /* У A4 нет собственного статуса ошибки: ПК получит таймаут. */
    if (!channel_open || job != PROXY_JOB_NONE || slave_tx_state != 0)
      break;

    memcpy(slave_tx, request + 1, length - 1);
    binex_transmitter_init(&slave_protocol, slave_tx, length - 1);
    slave_tx_state = 1;
    break;
  }
}

void BootloaderProxyProcess(void)
{
  ProxyPortResult_t result;

  /* Payload ответа нельзя менять, пока его читает передатчик ПК.
     Входной поток в это время сохраняет порт проекта. */
  if (response_length != 0)
    return;

  switch (job)
  {
  case PROXY_JOB_INIT:
    result = port_proxy_init_poll();
    if (result == PROXY_PORT_DONE)
    {
      initialized = 1;
      job = PROXY_JOB_NONE;
      set_status(CMD_PROXY_INIT_SLAVES, PROXY_STATUS_OK);
    }
    else if (result == PROXY_PORT_ERROR)
    {
      job = PROXY_JOB_NONE;
      set_status(CMD_PROXY_INIT_SLAVES, PROXY_STATUS_ERROR);
    }
    else if ((uint32_t)(SYSTICK_GET_VALUE() - keepalive_timer) >= PROXY_KEEPALIVE_MS)
    {
      keepalive_timer = SYSTICK_GET_VALUE();
      set_status(CMD_PROXY_INIT_SLAVES, PROXY_STATUS_CONTINUE);
    }
    return;

  case PROXY_JOB_LIST:
  {
    uint16_t count;
    ProxyListResult_t list_result = port_proxy_get_slave_list(
        list_offset, response + 2, sizeof(response) - 2, &count);

    if (list_result == PROXY_LIST_ERROR)
    {
      job = PROXY_JOB_NONE;
      set_status(CMD_PROXY_GET_SLAVE_LIST, PROXY_STATUS_ERROR);
    }
    else
    {
      response[0] = CMD_PROXY_GET_SLAVE_LIST;
      response[1] = list_result == PROXY_LIST_MORE ? PROXY_STATUS_CONTINUE : PROXY_STATUS_OK;
      response_length = 2 + count;
      list_offset += count;
      if (list_result == PROXY_LIST_DONE)
        job = PROXY_JOB_NONE;
    }
    return;
  }

  case PROXY_JOB_OPEN:
    result = port_proxy_open_poll();
    if (result != PROXY_PORT_BUSY)
    {
      channel_open = result == PROXY_PORT_DONE;
      job = PROXY_JOB_NONE;
      set_status(CMD_PROXY_OPEN, channel_open ? PROXY_STATUS_OK : PROXY_STATUS_ERROR);
    }
    return;

  case PROXY_JOB_CLOSE:
    result = port_proxy_close_poll();
    if (result != PROXY_PORT_BUSY)
    {
      job = PROXY_JOB_NONE;
      set_status(CMD_PROXY_CLOSE,
                 result == PROXY_PORT_DONE ? PROXY_STATUS_OK : PROXY_STATUS_ERROR);
    }
    return;
  }

  if (!channel_open)
    return;

  if (slave_tx_state == 1)
  {
    if (binex_transmit(&slave_protocol) == BINEX_PACK_TX)
      slave_tx_state = 2;
  }
  if (slave_tx_state == 2 && port_proxy_serial_transfer_completed())
    slave_tx_state = 0;

  if (binex_receiver(&slave_protocol, port_proxy_serial_getc()) == BINEX_PACK_RX)
  {
    uint16_t length = binex_get_rxpack_len(&slave_protocol);
    response[0] = CMD_PROXY_DATA;
    memcpy(response + 1, slave_rx, length);
    response_length = 1 + length;
  }
}

uint16_t BootloaderProxyGetResponse(uint8_t **payload)
{
  *payload = response;
  return response_length;
}

void BootloaderProxyResponseSent(void)
{
  response_length = 0;
}

#endif
