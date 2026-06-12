/*
 * protocol.h
 *
 * Wire protocol used between the STM32 hub and the host PC.
 *
 *   Command  (PC -> STM):  [0xAC][MASK_L][MASK_M][MASK_H][CRC]   5 bytes
 *   Fragment (STM -> PC):  [STX][IDX_H][IDX_L][PAYLOAD x16][CRC] 20 bytes
 *     STX = 0xAA  sensor fragment, more coming
 *     STX = 0xAF  sensor fragment, last
 *     STX = 0xA0  image fragment,  more coming
 *     STX = 0xA1  image fragment,  last
 *   ACK      (PC -> STM):  [0xFF]                                1 byte
 *
 * Pure framing only — no knowledge of sensors, cameras, or the hub
 * state machine. Safe to unit-test on a host with a mocked HAL_UART.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* -- Start-of-frame bytes -- */
#define STX_SENSOR_MORE  0xAA
#define STX_SENSOR_LAST  0xAF
#define STX_IMAGE_MORE   0xA0
#define STX_IMAGE_LAST   0xA1
#define STX_IMAGE_META_MORE 0xA2
#define STX_IMAGE_META_LAST 0xA3
#define STX_CMD          0xAC
#define STX_ACK          0xFF


/* -- Packet sizes -- */
#define CMD_LEN          5
#define FRAG_SIZE        20
#define PAYLOAD_BYTES    16

/**
 * @brief XOR checksum over @p len bytes of @p buf.
 */
uint8_t calc_crc(const uint8_t *buf, uint8_t len);

/**
 * @brief Split @p buf into 16-byte payload fragments and transmit them
 *        over UART1 with the given start-of-frame bytes.
 *
 * The last fragment uses @p stx_last; all others use @p stx_more.
 * Each fragment is exactly FRAG_SIZE bytes; the trailing payload is
 * zero-padded if @p len is not a multiple of PAYLOAD_BYTES.
 */
void send_fragments(const uint8_t *buf, uint32_t len,
                    uint8_t stx_more, uint8_t stx_last);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
