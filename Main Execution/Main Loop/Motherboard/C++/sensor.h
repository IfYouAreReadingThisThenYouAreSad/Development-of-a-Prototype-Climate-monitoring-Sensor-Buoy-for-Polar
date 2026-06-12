/*
 * sensor.h
 *
 * The Sensor interface — the contract every sensor implements.
 *
 * Adding a new sensor:
 *   1. Create xxx_driver.{hpp,cpp} (or .c) with the device-specific code.
 *      The driver knows about registers and datasheet maths only — it
 *      does NOT know about this struct or the hub.
 *   2. Create xxx_sensor.cpp (or .c) — the adapter. It owns a static
 *      driver instance and exports `const Sensor xxx_sensor` (with
 *      extern "C" linkage if it is a .cpp file).
 *   3. Add one extern declaration and one entry in REGISTRY[] inside
 *      sensor_registry.c.
 */

#ifndef SENSOR_H
#define SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Sensor adapter descriptor.
 *
 * Each sensor in the system is described by exactly one const Sensor
 * object. The hub iterates the registry, checks the command bitmask
 * against `bit`, and on a match calls `read()` to fill `size` bytes
 * into the response buffer.
 *
 * @field bit   Bit position in the command bitmask that requests this sensor.
 * @field size  Number of bytes this sensor writes into the payload buffer.
 * @field name  Diagnostic name — useful for logging, never sent on the wire.
 * @field init  Optional one-shot initialisation (called once after HAL is up).
 *              May be NULL if the sensor does not need initialisation.
 * @field read  Required. Writes exactly `size` bytes to @p out.
 */
typedef struct {
    uint8_t     bit;
    uint8_t     size;
    const char *name;
    void      (*init)(void);
    void      (*read)(uint8_t *out);
} Sensor;

#ifdef __cplusplus
}
#endif

#endif /* SENSOR_H */
