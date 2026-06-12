/*
 * db_hub.h
 *
 * Command framing and dispatch.
 *
 * Receives 5-byte commands from the host, validates them, then either
 * triggers a camera capture or builds a sensor response — with retries
 * on missing ACK.
 */

#ifndef DB_HUB_H
#define DB_HUB_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Arm UART1 for byte-by-byte interrupt reception.
 *
 * Must be called once after MX_USART1_UART_Init().
 */
void hub_begin(void);

/**
 * @brief Dispatch any pending command received from the host.
 *
 * Call from the main loop. Returns immediately if no command is ready.
 */
void hub_process(void);

/**
 * @brief Hook invoked from HAL_UART_RxCpltCallback() when a byte arrives.
 *
 * Exposed only so the callback can live in this module. Application
 * code does not need to call this directly.
 */
void hub_on_rx_byte(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_HUB_H */
