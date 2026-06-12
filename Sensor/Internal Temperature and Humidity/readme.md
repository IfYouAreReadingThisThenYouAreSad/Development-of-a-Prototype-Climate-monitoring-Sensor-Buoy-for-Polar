# Internal Temperature and Humidity Module (STM32)

If you are looking for libraries for **Arduino or Python** and information on the **SHT45 Temperature & Humidity Sensor**, please refer to the official Adafruit documentation:

https://learn.adafruit.com/adafruit-sht40-temperature-humidity-sensor/overview

---

# 1. Overview

This directory contains a driver for interfacing with the **Adafruit Sensirion SHT45 Precision Temperature & Humidity Sensor - STEMMA QT / Qwiic** over I2C using an STM32 microcontroller.

The SHT45 enables real-time measurement of:
- Temperature
- Humidity

This module is used on the buoy to monitor internal temperature inside the buoy and humidity inside the buoy for long term health monitoring and camera decisions.

---

# 2. Hardware

The system uses the:

**SHT45 Precision Temperature & Humidity Sensor**

### Product Link
https://thepihut.com/products/adafruit-sensirion-sht45-precision-temperature-humidity-sensor-stemma-qt-qwiic

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/Sensor/Internal%20Temperature%20and%20Humidity/adafruit-sensirion-sht45-precision-temperature-humidity-sensor-stemma-qt-qwiic-the-pi-hut-ada5665-40059654996163_900x.jpg" width="400">
</p>

<p align="center"><b>Figure 1:</b> SHT45 Precision Temperature & Humidity Sensor</p>

---

# 3. Hardware Connections

The SHT45 communicates via the **I2C interface**.

## STM32 ↔ SHT45 Connections

| SHT45 Pin | STM32 / System | Description       |
|-----------|-----------------|-------------------|
| VCC       | 3.3V / 5V       | Module power      |
| GND       | GND             | Common ground     |
| SCL       | I2C_SCL         | I2C clock line    |
| SDA       | I2C_SDA         | I2C data line     |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function  |
|-----------|------------|-----------|
| PB6       | I2C1_SCL   | I2C clock |
| PB7       | I2C1_SDA   | I2C data  |

---

# 4. API

## Initialisation

| Function | Description |
|----------|-------------|
| `bool SHT4x_Init(I2C_HandleTypeDef *hi2c)` | Initialises the driver with the STM32 I2C peripheral, sets default address/precision/heater settings, and performs a soft reset. Returns `true` on success. |

---

## Important `#define` Constants

These constants are held in `SHT4x.h` and define the I2C address, command bytes, and timeout used by the driver. It may be worth reviewing these before compiling for a different project.

| Variable | Description |
|----------|-------------|
| `SHT4X_DEFAULT_ADDR` | 7-bit I2C address (0x44) shifted for HAL use. |
| `SHT4X_I2C_TIMEOUT_MS` | HAL I2C timeout in milliseconds, used for all I2C transmit/receive calls. |
| `SHT4X_CMD_NOHEAT_HIGH/MED/LOW` | Measurement commands for high/medium/low precision with the heater disabled. |
| `SHT4X_CMD_*HEAT_*` | Measurement commands that enable the on-chip heater at high/medium/low power for 1 s or 100 ms, used for de-icing/de-humidifying the sensor. |
| `SHT4X_CMD_READ_SERIAL` | Command to read the sensor's 32-bit serial number. |
| `SHT4X_CMD_SOFT_RESET` | Command to perform a soft reset of the sensor. |

---

## Public Functions

| Function | Description |
|----------|-------------|
| `bool SHT4x_Init(I2C_HandleTypeDef *hi2c)` | Initialises the driver and performs a soft reset of the sensor. |
| `bool SHT4x_Reset(void)` | Performs a soft reset of the sensor. |
| `bool SHT4x_ReadSerial(uint32_t *serial)` | Reads the sensor's 32-bit serial number with CRC verification. |
| `void SHT4x_SetPrecision(SHT4x_Precision_t precision)` | Sets the measurement precision (affects conversion time and accuracy). |
| `SHT4x_Precision_t SHT4x_GetPrecision(void)` | Returns the current precision setting. |
| `void SHT4x_SetHeater(SHT4x_Heater_t heater)` | Sets the on-chip heater mode, used for de-icing/de-humidifying the sensor. |
| `SHT4x_Heater_t SHT4x_GetHeater(void)` | Returns the current heater mode. |
| `bool SHT4x_GetEvent(SHT4x_Data_t *data)` | Triggers a measurement and returns temperature (°C) and humidity (%RH) in `data`. |

---

# 5. Enums and Structs

## `SHT4x_Precision_t`

Sets the measurement precision, which affects the conversion time and accuracy of each reading.

| Value | Description |
|-------|-------------|
| `SHT4X_HIGH_PRECISION` | Highest accuracy, ~10 ms conversion time. |
| `SHT4X_MED_PRECISION` | Medium accuracy, ~5 ms conversion time. |
| `SHT4X_LOW_PRECISION` | Lowest accuracy, ~2 ms conversion time. |

## `SHT4x_Heater_t`

Sets the on-chip heater, used to clear condensation or ice from the sensor in cold/humid environments.

| Value | Description |
|-------|-------------|
| `SHT4X_NO_HEATER` | Heater disabled (default, normal operation). |
| `SHT4X_HIGH_HEATER_1S` | High heater power, 1 second pulse. |
| `SHT4X_HIGH_HEATER_100MS` | High heater power, 100 ms pulse. |
| `SHT4X_MED_HEATER_1S` | Medium heater power, 1 second pulse. |
| `SHT4X_MED_HEATER_100MS` | Medium heater power, 100 ms pulse. |
| `SHT4X_LOW_HEATER_1S` | Low heater power, 1 second pulse. |
| `SHT4X_LOW_HEATER_100MS` | Low heater power, 100 ms pulse. |

## `SHT4x_Data_t`

Struct used to return sensor readings.

| Member | Description |
|--------|-------------|
| `float temperature` | Temperature in degrees Celsius. |
| `float humidity` | Relative humidity in %RH, clamped to the range 0–100. |

---

# 6. Code Flow

## Processing Steps

1. **Initialisation (run once)**
   - Resets internal driver state
   - Sets default I2C address, precision, and heater mode
   - Performs a soft reset of the sensor

2. **Measurement**
   - Selects the measurement command and conversion delay based on the configured precision and heater mode
   - Sends the measurement command over I2C
   - Waits for the conversion time to elapse
   - Reads 6 bytes back: `[T_MSB, T_LSB, T_CRC, RH_MSB, RH_LSB, RH_CRC]`
   - Verifies the CRC-8 checksum for both the temperature and humidity values

3. **Conversion**
   - Converts raw temperature ticks to degrees Celsius: `T = -45 + 175 * (ticks / 65535)`
   - Converts raw humidity ticks to %RH: `RH = -6 + 125 * (ticks / 65535)`
   - Clamps humidity to the range 0–100 %RH
   - Stores the results internally and returns them via the output struct

---

# 7. Dependencies

- STM32 HAL Library
- STM32 I2C Peripheral

The correct HAL header is included directly in `SHT4x.h` (`stm32f4xx_hal.h`). If targeting a different STM32 family, update this include accordingly (e.g. `stm32l4xx_hal.h`, `stm32h7xx_hal.h`).

---

# 8. Example Usage (C)

```c
SHT4x_Data_t debug_data = {0};

if (!SHT4x_Init(&hi2c3)) {
    Error_Handler();
}

while (1)
{
    if (SHT4x_GetEvent(&debug_data))
    {
        float debug_temperature = debug_data.temperature;
        float debug_humidity    = debug_data.humidity;
    }

    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(500);
}
```
