/*
 * protocol.c
 *
 * Implementation of the wire protocol declared in protocol.h.
 */

#include "protocol.h"
#include "main.h"
#include <string.h>

uint8_t calc_crc(const uint8_t *buf, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= buf[i];
    }
    return crc;
}

void send_fragments(const uint8_t *buf, uint32_t len,
                    uint8_t stx_more, uint8_t stx_last)
{
    uint32_t n_frags = (len + PAYLOAD_BYTES - 1) / PAYLOAD_BYTES;
    if (n_frags == 0) n_frags = 1;

    for (uint32_t i = 0; i < n_frags; i++)
    {
        uint32_t offset  = i * PAYLOAD_BYTES;
        uint8_t  is_last = (i == n_frags - 1);
        uint8_t  chunk   = is_last ? (uint8_t)(len - offset) : PAYLOAD_BYTES;

        uint8_t pkt[FRAG_SIZE] = {0};
        pkt[0]  = is_last ? stx_last : stx_more;
        pkt[1]  = (i >> 8) & 0xFF;
        pkt[2]  = (i     ) & 0xFF;
        memcpy(&pkt[3], buf + offset, chunk);
        pkt[19] = calc_crc(pkt, 19);

        HAL_UART_Transmit(&huart1, pkt, FRAG_SIZE, 100);
        HAL_Delay(20);
    }
}
