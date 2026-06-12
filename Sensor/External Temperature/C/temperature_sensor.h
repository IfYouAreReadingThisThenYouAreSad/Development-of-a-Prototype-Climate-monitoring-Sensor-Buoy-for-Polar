#pragma once
/*
 * temperature_sensor.h — DB thermistor driver
 *
 * Four PS103J2 thermistors on ADC1_IN10–IN13 (PC0–PC3).
 * Payload: 4 × float32 = 16 bytes, temperatures in °C.
 * Channels that fail emit TEMPERATURE_ERROR_SENTINEL.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEMPERATURE_ERROR_SENTINEL  (-99.9f)

/**
 * @brief  Overrides MX_ADC1_Init() scan config to single-conversion mode,
 *         which is required for per-channel dynamic switching in temp_sensor_read().
 *         Call after MX_ADC1_Init() and before sensors_init().
 */
void temp_sensor_init(void);

/**
 * @brief  Reads all four thermistor channels sequentially.
 * @param  out      Output buffer, must be at least 16 bytes.
 * @param  len_out  Set to 16 on return. May be NULL.
 */
void temp_sensor_read(uint8_t *out, uint8_t *len_out);

#ifdef __cplusplus
}
#endif
