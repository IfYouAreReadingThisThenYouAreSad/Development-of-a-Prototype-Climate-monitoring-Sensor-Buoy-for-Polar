/*
 * db_sensor_registry.h
 *
 * Central list of every Sensor in the system. The hub talks to this
 * module — it does not know which sensors exist or what they do.
 */

#ifndef DB_SENSOR_REGISTRY_H
#define DB_SENSOR_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Call every registered sensor's init() function (if non-NULL).
 *
 * Must be called after HAL_Init() and all relevant MX_*_Init() functions
 * — drivers may call HAL_I2C / HAL_Delay during init.
 */
void sensors_init(void);

/**
 * @brief Build the sensor response payload for a given command bitmask.
 *
 * Walks the registry in order. For every sensor whose `bit` is set in
 * @p mask, calls its read() to append `size` bytes to @p buf.
 *
 * @param mask  Command bitmask received from the host.
 * @param buf   Output buffer — must be large enough for the worst case
 *              (sum of all sensor `size` fields). The hub allocates 64 bytes.
 * @return Number of bytes written to @p buf.
 */
uint16_t build_sensor_payload(uint32_t mask, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* DB_SENSOR_REGISTRY_H */
