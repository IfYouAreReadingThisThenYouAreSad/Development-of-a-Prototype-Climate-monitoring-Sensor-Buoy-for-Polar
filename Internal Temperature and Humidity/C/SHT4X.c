/*
 * SHT4x.c
 *
 *  Created on: 5 Jun 2026
 *      Author: thomasneedham
 *
 * @file SHT4x.c
 * @brief STM32 HAL driver for the Sensirion SHT4x Temperature & Humidity Sensor
 *
 * Ported from Adafruit's Arduino library (BSD licence) to STM32 HAL.
 * https://github.com/adafruit/Adafruit_SHT4X/blob/master/Adafruit_SHT4x.cpp
 *
 */

#include "SHT4x.h"
#include <string.h>   /* memset */

/* -------------------------------------------------------------------------
 * Private handle — hidden inside this file, never exposed to the user
 * ---------------------------------------------------------------------- */

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint16_t           i2c_addr;
    SHT4x_Precision_t  precision;
    SHT4x_Heater_t     heater;
    float              last_temp;
    float              last_humidity;
} SHT4x_Handle_t;

static SHT4x_Handle_t _sht;   /* single private instance */

/* -------------------------------------------------------------------------
 * Private helpers
 * ---------------------------------------------------------------------- */

/**
 * CRC-8 used by Sensirion:
 *   Polynomial : 0x31  (x8 + x5 + x4 + 1)
 *   Init value : 0xFF
 *   Final XOR  : 0x00
 *   Test vector: 0xBE 0xEF -> 0x92
 */
static uint8_t sht4x_crc8(const uint8_t *data, uint8_t len)
{
    const uint8_t POLY = 0x31;
    uint8_t crc = 0xFF;

    for (uint8_t j = 0; j < len; j++) {
        crc ^= data[j];
        for (uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ POLY) : (crc << 1);
        }
    }
    return crc;
}

/** Write a single command byte to the sensor. */
static bool sht4x_write_cmd(uint8_t cmd)
{
    return HAL_I2C_Master_Transmit(_sht.hi2c,
                                   _sht.i2c_addr,
                                   &cmd, 1,
                                   SHT4X_I2C_TIMEOUT_MS) == HAL_OK;
}

/** Read len bytes from the sensor into buf. */
static bool sht4x_read(uint8_t *buf, uint8_t len)
{
    return HAL_I2C_Master_Receive(_sht.hi2c,
                                  _sht.i2c_addr,
                                  buf, len,
                                  SHT4X_I2C_TIMEOUT_MS) == HAL_OK;
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

bool SHT4x_Init(I2C_HandleTypeDef *hi2c)
{
    if (!hi2c) return false;

    memset(&_sht, 0, sizeof(_sht));
    _sht.hi2c      = hi2c;
    _sht.i2c_addr  = SHT4X_DEFAULT_ADDR;
    _sht.precision = SHT4X_HIGH_PRECISION;
    _sht.heater    = SHT4X_NO_HEATER;

    return SHT4x_Reset();
}

bool SHT4x_Reset(void)
{
    if (!sht4x_write_cmd(SHT4X_CMD_SOFT_RESET)) return false;
    HAL_Delay(1);
    return true;
}

bool SHT4x_ReadSerial(uint32_t *serial)
{
    uint8_t buf[6];

    if (!sht4x_write_cmd(SHT4X_CMD_READ_SERIAL)) return false;
    HAL_Delay(10);
    if (!sht4x_read(buf, 6)) return false;

    /* Verify both CRCs */
    if (sht4x_crc8(buf,     2) != buf[2]) return false;
    if (sht4x_crc8(buf + 3, 2) != buf[5]) return false;

    *serial  = (uint32_t)buf[0] << 24;
    *serial |= (uint32_t)buf[1] << 16;
    *serial |= (uint32_t)buf[3] <<  8;
    *serial |=            buf[4];

    return true;
}

void SHT4x_SetPrecision(SHT4x_Precision_t precision)
{
    _sht.precision = precision;
}

SHT4x_Precision_t SHT4x_GetPrecision(void)
{
    return _sht.precision;
}

void SHT4x_SetHeater(SHT4x_Heater_t heater)
{
    _sht.heater = heater;
}

SHT4x_Heater_t SHT4x_GetHeater(void)
{
    return _sht.heater;
}

bool SHT4x_GetEvent(SHT4x_Data_t *data)
{
    uint8_t  cmd      = SHT4X_CMD_NOHEAT_HIGH;
    uint16_t delay_ms = 10;
    uint8_t  buf[6];

    /* ---- Select command & conversion time -------------------------------- */
    if (_sht.heater == SHT4X_NO_HEATER) {
        switch (_sht.precision) {
            case SHT4X_HIGH_PRECISION: cmd = SHT4X_CMD_NOHEAT_HIGH; delay_ms =   10; break;
            case SHT4X_MED_PRECISION:  cmd = SHT4X_CMD_NOHEAT_MED;  delay_ms =    5; break;
            case SHT4X_LOW_PRECISION:  cmd = SHT4X_CMD_NOHEAT_LOW;  delay_ms =    2; break;
        }
    } else {
        switch (_sht.heater) {
            case SHT4X_HIGH_HEATER_1S:    cmd = SHT4X_CMD_HIGHHEAT_1S;    delay_ms = 1100; break;
            case SHT4X_HIGH_HEATER_100MS: cmd = SHT4X_CMD_HIGHHEAT_100MS; delay_ms =  110; break;
            case SHT4X_MED_HEATER_1S:     cmd = SHT4X_CMD_MEDHEAT_1S;     delay_ms = 1100; break;
            case SHT4X_MED_HEATER_100MS:  cmd = SHT4X_CMD_MEDHEAT_100MS;  delay_ms =  110; break;
            case SHT4X_LOW_HEATER_1S:     cmd = SHT4X_CMD_LOWHEAT_1S;     delay_ms = 1100; break;
            case SHT4X_LOW_HEATER_100MS:  cmd = SHT4X_CMD_LOWHEAT_100MS;  delay_ms =  110; break;
            default: break;
        }
    }

    /* ---- Issue command --------------------------------------------------- */
    if (!sht4x_write_cmd(cmd)) return false;
    HAL_Delay(delay_ms);

    /* ---- Read 6 bytes: [T_MSB, T_LSB, T_CRC, RH_MSB, RH_LSB, RH_CRC] --- */
    if (!sht4x_read(buf, 6)) return false;

    /* ---- Verify CRCs ----------------------------------------------------- */
    if (sht4x_crc8(buf,     2) != buf[2]) return false;
    if (sht4x_crc8(buf + 3, 2) != buf[5]) return false;

    /* ---- Convert raw ticks to physical values ----------------------------- */
    float t_ticks  = (uint16_t)buf[0] * 256u + buf[1];
    float rh_ticks = (uint16_t)buf[3] * 256u + buf[4];

    float temperature = -45.0f + 175.0f * (t_ticks  / 65535.0f);
    float humidity    =  -6.0f + 125.0f * (rh_ticks / 65535.0f);

    /* Clamp humidity to [0, 100] */
    if (humidity < 0.0f)   humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    /* ---- Store & return -------------------------------------------------- */
    _sht.last_temp     = temperature;
    _sht.last_humidity = humidity;

    if (data) {
        data->temperature = temperature;
        data->humidity    = humidity;
    }

    return true;
}
