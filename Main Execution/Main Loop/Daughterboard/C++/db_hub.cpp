/*
 * db_hub.c
 *
 * Mirrors MB hub.cpp in structure; the differences are:
 *
 *   DB hub (slave):
 *   ─────────────────────────────
 *   Receives commands from MB
 *   Sends fragments to MB
 *   No ACK/retry (MB owns that)
 *   No camera (cameras are on MB)
 *   Uses UART for communications (inter-board)
 *
 *   The ISR → volatile flag → main-loop pattern is identical to the MB,
 *   so the same reasoning about thread safety applies.
 */

#include "db_hub.h"
#include "main.h"
#include "db_protocol.h"
#include "db_sensor_registry.h"
#include <stdint.h>

/* -- ISR-touched state -- */
static volatile uint8_t  hub_cmd_ready = 0;
static volatile uint32_t hub_cmd_mask  = 0;

/* -- Main-loop state -- */
static uint8_t hub_rx_byte              = 0;
static uint8_t hub_frame_buf[CMD_LEN];
static uint8_t hub_frame_index          = 0;
static uint8_t hub_resp_buf[128];

/* =========================================================================
 * hub_begin
 * Arm the inter-board UART for byte-by-byte interrupt reception.
 * Called once from main() after MX_USARTx_UART_Init().
 * =========================================================================*/
void hub_begin(void)
{
    HAL_UART_Receive_IT(&huart2, &hub_rx_byte, 1);
}


/* =========================================================================
 * hub_on_rx_byte
 * Called from HAL_UART_RxCpltCallback on every received byte.
 * Builds a 5-byte command frame; validates STX + CRC before accepting.
 *
 * Frame structure (identical to MB):
 *   [0xAC][MASK_L][MASK_M][MASK_H][CRC]
 * =========================================================================*/
void hub_on_rx_byte(void)
{

    /* Restart frame on STX_CMD to start fresh frame. */
    if (hub_rx_byte == STX_CMD) {
        hub_frame_index = 0;
    }

    if (hub_frame_index < CMD_LEN) {
        hub_frame_buf[hub_frame_index++] = hub_rx_byte;
    }

    if (hub_frame_index == CMD_LEN) {
        uint8_t expected = calc_crc(hub_frame_buf, CMD_LEN - 1);
        if (hub_frame_buf[0] == STX_CMD && hub_frame_buf[4] == expected) {
            hub_cmd_mask  = (uint32_t)hub_frame_buf[1]
                          | ((uint32_t)hub_frame_buf[2] << 8)
                          | ((uint32_t)hub_frame_buf[3] << 16);
            hub_cmd_ready = 1;
        }
        hub_frame_index = 0;
    }

    HAL_UART_Receive_IT(&huart2, &hub_rx_byte, 1);
}


/* =========================================================================
 * hub_process
 * Called from the main loop. Returns immediately if no command is ready.
 * On a valid command:
 *   1. Builds the sensor payload via the registry.
 *   2. Sends the payload back to the MB as 20-byte fragments.
 *   3. Does NOT wait for ACK — the MB manages retries.
 * =========================================================================*/
void hub_process(void)
{
    if (!hub_cmd_ready) {
        return;
    }

    /* Snapshot volatile state with interrupts disabled */
    __disable_irq();
    uint32_t mask    = hub_cmd_mask;
    hub_cmd_ready    = 0;
    __enable_irq();

    if (mask == 0) {
        return;
    }

    uint16_t len = build_sensor_payload(mask, hub_resp_buf);

    /* Only respond if at least one sensor matched the mask.
     * Sending an empty fragment when len==0 would confuse the MB's
     * blocking HAL_UART_Receive, so we stay silent instead. */
    send_fragments(hub_resp_buf, len, STX_SENSOR_MORE, STX_SENSOR_LAST);
}

/* =========================================================================
 * HAL_UART_RxCpltCallback
 * Dispatches to hub_on_rx_byte() on the inter-board UART.
 * Defined extern "C" so the C HAL can call it from the C++ translation unit.
 * If a second UART RX interrupt is ever needed on the DB, add another
 * else-if branch here.
 * -------------------------------------------------------------------------- */
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        hub_on_rx_byte();
    }
}
