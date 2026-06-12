/*
 * SHT4x.h
 *
 *  Created on: 5 Jun 2026
 *      Author: thomasneedham
 *
 *
 * @file SHT4x.h
 * @brief STM32 HAL driver for the Sensirion SHT4x Temperature & Humidity Sensor
 *
 * Ported from Adafruit's Arduino library to STM32 HAL (I2C).
 * https://github.com/adafruit/Adafruit_SHT4X/blob/master/Adafruit_SHT4x.cpp
 * Compatible with SHT40, SHT41, SHT45.
 *
 * Usage:
 *   SHT4x_Init(&hi2c3);
 *   SHT4x_GetEvent(&data);
 */

#ifndef INC_SHT4X_H_
#define INC_SHT4X_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"   /* <-- Change XX to your STM32 family, e.g. f4, l4, h7 */
#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * I2C address & command bytes
 * ---------------------------------------------------------------------- */
#define SHT4X_DEFAULT_ADDR          (0x44 << 1)  /**< 7-bit addr shifted for HAL */

#define SHT4X_CMD_NOHEAT_HIGH       0xFD  /**< High precision, no heater        */
#define SHT4X_CMD_NOHEAT_MED        0xF6  /**< Medium precision, no heater      */
#define SHT4X_CMD_NOHEAT_LOW        0xE0  /**< Low precision, no heater         */

#define SHT4X_CMD_HIGHHEAT_1S       0x39  /**< High heat, 1 s measurement       */
#define SHT4X_CMD_HIGHHEAT_100MS    0x32  /**< High heat, 100 ms measurement    */
#define SHT4X_CMD_MEDHEAT_1S        0x2F  /**< Med heat,  1 s measurement       */
#define SHT4X_CMD_MEDHEAT_100MS     0x24  /**< Med heat,  100 ms measurement    */
#define SHT4X_CMD_LOWHEAT_1S        0x1E  /**< Low heat,  1 s measurement       */
#define SHT4X_CMD_LOWHEAT_100MS     0x15  /**< Low heat,  100 ms measurement    */

#define SHT4X_CMD_READ_SERIAL       0x89  /**< Read serial number               */
#define SHT4X_CMD_SOFT_RESET        0x94  /**< Soft reset                       */

#define SHT4X_I2C_TIMEOUT_MS        100   /**< HAL I2C timeout (ms)             */

/* -------------------------------------------------------------------------
 * Enumerations
 * ---------------------------------------------------------------------- */

/** Measurement precision (affects conversion time) */
typedef enum {
    SHT4X_HIGH_PRECISION = 0,
    SHT4X_MED_PRECISION,
    SHT4X_LOW_PRECISION,
} SHT4x_Precision_t;

/** Optional on-chip heater setting */
typedef enum {
    SHT4X_NO_HEATER = 0,
    SHT4X_HIGH_HEATER_1S,
    SHT4X_HIGH_HEATER_100MS,
    SHT4X_MED_HEATER_1S,
    SHT4X_MED_HEATER_100MS,
    SHT4X_LOW_HEATER_1S,
    SHT4X_LOW_HEATER_100MS,
} SHT4x_Heater_t;

/* -------------------------------------------------------------------------
 * Data structures
 * ---------------------------------------------------------------------- */

/** Sensor reading result */
typedef struct {
    float temperature;        /**< Temperature in degrees C       */
    float humidity;           /**< Relative humidity in %RH       */
} SHT4x_Data_t;

/* -------------------------------------------------------------------------
 * Function prototypes
 * ---------------------------------------------------------------------- */

/**
 * @brief Initialise the driver and reset the sensor.
 * @param hi2c   Pointer to a configured HAL I2C handle (e.g. &hi2c3).
 * @return true on success, false if the sensor did not respond.
 */
bool SHT4x_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Perform a soft reset of the sensor.
 * @return true on success.
 */
bool SHT4x_Reset(void);

/**
 * @brief Read the 32-bit serial number from the sensor.
 * @param serial  Output: serial number.
 * @return true on success.
 */
bool SHT4x_ReadSerial(uint32_t *serial);

/** @brief Set measurement precision. */
void SHT4x_SetPrecision(SHT4x_Precision_t precision);

/** @brief Get current precision setting. */
SHT4x_Precision_t SHT4x_GetPrecision(void);

/** @brief Set heater mode. */
void SHT4x_SetHeater(SHT4x_Heater_t heater);

/** @brief Get current heater mode. */
SHT4x_Heater_t SHT4x_GetHeater(void);

/**
 * @brief Trigger a measurement and return temperature + humidity.
 * @param data  Output: temperature (degrees C) and humidity (%RH).
 * @return true on success, false on I2C error or CRC mismatch.
 */
bool SHT4x_GetEvent(SHT4x_Data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* INC_SHT4X_H_ */
