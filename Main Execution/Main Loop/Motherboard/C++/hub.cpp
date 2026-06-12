/*
 * hub.c
 *
 * Command framing and dispatch state machine.
 *
 * Variables touched from the UART RX ISR are `volatile`; they are read
 * with interrupts briefly disabled in hub_process() to take a consistent
 * snapshot.
 */

#include "hub.h"
#include "main.h"
#include "protocol.h"
#include "sensor_registry.h"
#include "camera.h"
#include <stdint.h>

/* -- Tunables -- */
#define ACK_TIMEOUT_MS  2000
#define CAMERA1_BIT		1
#define CAMERA2_BIT     2
#define MAX_RETRIES     3

/* -- ISR-touched state -- */
static volatile uint8_t  hub_cmd_ready    = 0;
static volatile uint8_t  hub_ack_received = 0;
static volatile uint32_t hub_cmd_mask     = 0;

/* -- Main-loop state -- */
static uint8_t hub_rx_byte     = 0;
static uint8_t hub_frame_buf[CMD_LEN];
static uint8_t hub_frame_index = 0;
static uint8_t hub_resp_buf[128];

void hub_begin(void)
{
    HAL_UART_Receive_IT(&huart1, &hub_rx_byte, 1);
}


void hub_on_rx_byte(void)
{
    /* Standalone ACK byte does not belong to a command frame. */
    if (hub_rx_byte == STX_ACK) {
        hub_ack_received = 1;
        hub_frame_index  = 0;
        HAL_UART_Receive_IT(&huart1, &hub_rx_byte, 1);
        return;
    }

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

    HAL_UART_Receive_IT(&huart1, &hub_rx_byte, 1);
}


/**
 * @brief Send len bytes from buf, retrying up to MAX_RETRIES times
 *        if the host does not ACK within ACK_TIMEOUT_MS.
 */
static void send_with_retry(const uint8_t *buf, uint16_t len,
                            uint8_t stx_more, uint8_t stx_last)
{
    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
        send_fragments(buf, len, stx_more, stx_last);
        uint32_t t0 = HAL_GetTick();
        while (!hub_ack_received && (HAL_GetTick() - t0) < ACK_TIMEOUT_MS) {

        }
        if (hub_ack_received) {
            break;
        }
    }
}

// forward bitmask to daughterboard

static uint16_t db_forward(uint32_t mask, uint8_t *out)
{
    uint8_t cmd[CMD_LEN];
    cmd[0] = STX_CMD;
    cmd[1] = (mask      ) & 0xFF;
    cmd[2] = (mask >>  8) & 0xFF;
    cmd[3] = (mask >> 16) & 0xFF;
    cmd[4] = calc_crc(cmd, 4);

    if (HAL_UART_Transmit(&huart6, cmd, CMD_LEN, 100) != HAL_OK) return 0;

    uint16_t total = 0;
    uint8_t  frag[FRAG_SIZE];

    for (;;) {
        if (HAL_UART_Receive(&huart6, frag, FRAG_SIZE, 500) != HAL_OK) break;

        uint8_t stx = frag[0];
        if ((stx != STX_SENSOR_MORE && stx != STX_SENSOR_LAST) ||
            frag[FRAG_SIZE - 1] != calc_crc(frag, FRAG_SIZE - 1)) break;

        uint16_t chunk = (stx == STX_SENSOR_LAST) ? frag[2] : PAYLOAD_BYTES;
        memcpy(out + total, &frag[3], chunk);
        total += chunk;
        if (stx == STX_SENSOR_LAST) break;
    }
    return total;
}



void hub_process(void)
{
    if (!hub_cmd_ready) {
        return;
    }

    /* Snapshot the volatile state with interrupts off, then re-enable. */
    __disable_irq();
    uint32_t mask    = hub_cmd_mask;
    hub_cmd_ready    = 0;
    hub_ack_received = 0;
    __enable_irq();

    mask &= ~(1UL << 23);   /* strip standby bit */
    if (mask == 0) {
        return;
    }

    if (mask & (1UL << CAMERA1_BIT)) {
    	for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    		send_jpeg_image(&cam1);
    		uint32_t t0 = HAL_GetTick();
    		while (!hub_ack_received && (HAL_GetTick() - t0) < ACK_TIMEOUT_MS) {
    		}
    		if (hub_ack_received) {
    			break;
    		}
    	}
    }

	hub_ack_received = 0;

    /* CAM 2 */
    if (mask & (1UL << CAMERA2_BIT)) {
        for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
            send_jpeg_image(&cam2);
            uint32_t t0 = HAL_GetTick();
            while (!hub_ack_received && (HAL_GetTick() - t0) < ACK_TIMEOUT_MS) {
                /* spin */
            }
            if (hub_ack_received) {
                break;
            }
        }
    }

    /* SENSORS — only if the camera bit is not set. */
    if (!(mask & (1UL << CAMERA1_BIT)) && !(mask & (1UL << CAMERA2_BIT))) {
        uint16_t len  = build_sensor_payload(mask, hub_resp_buf);
        len += db_forward(mask, hub_resp_buf + len);
        if (len > 0)
            send_with_retry(hub_resp_buf, len, STX_SENSOR_MORE, STX_SENSOR_LAST);
    }

}


/* -- HAL UART RX-complete callback ----------------------------------------
 *
 * Called every time one byte lands in interrupt.
 * -------------------------------------------------------------------------- */
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        hub_on_rx_byte();
    }
}
