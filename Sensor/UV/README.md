# UV Module (STM32)

If you are looking for libraries for **Arduino or Raspberry Pi** and information on the **LTR390-UV Ultraviolet Sensor**, please refer to the official Waveshare Wiki documentation:

https://www.waveshare.com/wiki/UV_Sensor_(C)

---

# 1. Overview

This directory contains a driver for interfacing with the **Digital LTR390-UV Ultraviolet Sensor** over I2C using an STM32 microcontroller.

The LTR390 enables real-time measurement of:
- Ambient light using the onboard ambient light sensor (ALS)
- Ultraviolet light

This module is used on the buoy to monitor UV light across seasons to help monitor climate change.

---

# 2. Hardware

The system uses the:

**Digital LTR390-UV Ultraviolet Sensor**

### Product Link
https://thepihut.com/products/digital-ltr390-uv-ultraviolet-sensor

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/UV/digital-ltr390-uv-ultraviolet-sensor-waveshare-wav-20467-41114641662147_1000x.webp" width="400">
</p>

<p align="center"><b>Figure 1:</b> UV Sensor Module</p>

---

# 3. Hardware Connections

The LTR390 communicates via the **I2C interface**.

## STM32 ↔ LTR390 Connections

| LTR390 Pin | STM32 / System | Description       |
|------------|----------------|-------------------|
| VCC        | 3.3V / 5V      | Module power      |
| GND        | GND            | Common ground     |
| SCL        | I2C_SCL        | I2C clock line    |
| SDA        | I2C_SDA        | I2C data line     |

---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function  |
|-----------|------------|-----------|
| PB6       | I2C1_SCL   | I2C clock |
| PB7       | I2C1_SDA   | I2C data  |

---

# 4. API

## Constructor

| Function | Description |
|----------|-------------|
| `Ltr390Driver(I2C_HandleTypeDef *hi2c_)` | Initialises the driver with the STM32 I2C peripheral. Internal variables are reset during construction. |

---

## Important `constexpr` Class Variables

These variables are held constant at compile time and are currently set for this project's application. It may be worth reviewing these before compiling for a different project.

These constants are private members of the class in the `.hpp` file and are intentionally hidden from the user to prevent accidental modification.

| Variable | Description |
|----------|-------------|
| `WINDOW_FACTOR` | Compensates for UV loss through the buoy enclosure window. Set to 1.0 until the window material has been calibrated. |
| `DATA_READY_TIMEOUT_MS` | Maximum time the driver waits for the sensor to be ready before returning a `TIMEOUT` error. Used in `wait_data_ready()`. |
| `MEAS_RATE_DEFAULT` | Controls resolution and measurement rate. Bits [6:4] set resolution, bits [2:0] set rate. `ALS_INTEGRATION_TIME` updates automatically when this changes. |
| `GAIN` | Sets the photodiode gain. `ALS_GAIN` updates automatically when this changes. |
| `COUNTS_PER_UVI` | Reference sensitivity constant used to convert raw UV counts to UV Index. Must be recalculated from the datasheet if `GAIN` or `MEAS_RATE_DEFAULT` are changed. |

---

## Public Functions

| Function | Description |
|----------|-------------|
| `Ltr390Status init()` | Initialises the sensor by probing the I2C bus, setting resolution, sampling rate, and gain, then enabling the sensor in UV mode. |
| `Ltr390Status calculate_uv_index()` | Reads the UV sensor and calculates the UV Index. Result is stored internally and retrieved with `get_uv_index()`. |
| `Ltr390Status calculate_lux()` | Reads the ambient light sensor and calculates lux. Result is stored internally and retrieved with `get_lux()`. |
| `float get_uv_index()` | Returns the last UV Index value computed by `calculate_uv_index()`. |
| `float get_lux()` | Returns the last lux value computed by `calculate_lux()`. |

---

# 5. Enums

## `Ltr390Status`

Returned by all driver functions for error handling and debugging.

| Value | Description |
|-------|-------------|
| `Ltr390Status::OK` | Operation successful. |
| `Ltr390Status::I2C_NO_ACKNOWLEDGMENT` | Sensor did not ACK — check wiring, power, and I2C address. |
| `Ltr390Status::I2C_TRANSMIT_ERROR` | I2C failure when transmitting to the sensor. |
| `Ltr390Status::I2C_RECEIVE_ERROR` | I2C failure when receiving data from the sensor. |
| `Ltr390Status::TIMEOUT` | Sensor did not signal data ready within `DATA_READY_TIMEOUT_MS`. |
| `Ltr390Status::INVALID_ARGUMENT` | Invalid `WINDOW_FACTOR` value (must be greater than 0). |
| `Ltr390Status::RESOLUTION_SETUP_ERROR` | I2C error when writing the resolution / measurement rate register. |
| `Ltr390Status::GAIN_SETUP_ERROR` | I2C error when writing the gain register. |
| `Ltr390Status::UV_MODE_SWITCH_ERROR` | Failed to switch the sensor to UV measurement mode. |
| `Ltr390Status::ALS_MODE_SWITCH_ERROR` | Failed to switch the sensor to ambient light measurement mode. |

---

# 6. Code Flow

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/UV/UV%20Sensor%20block%20diagram.jpeg" width="800">
</p>

<p align="center"><b>Figure 2:</b> LTR390 Processing Pipeline for Calculating UV Index and Lux</p>

## Processing Steps

1. **Initialisation (run once)**
   - Probes the I2C bus to confirm the sensor is present
   - Sets resolution and sampling rate
   - Sets gain
   - Enables the sensor in UV mode

2. **UV Measurement**
   - Switches sensor to UV mode
   - Waits until the sensor signals data is ready
   - Reads raw 24-bit UV count
   - Calculates UV Index: `UVI = (counts / COUNTS_PER_UVI) * WINDOW_FACTOR`

3. **Lux Measurement**
   - Switches sensor to ALS mode
   - Waits until the sensor signals data is ready
   - Reads raw 24-bit ALS count
   - Calculates lux: `Lux = (0.6 * counts) / (gain * integration_time)`

---

# 7. Dependencies

- STM32 HAL Library
- STM32 I2C Peripheral

The correct HAL header is selected automatically at compile time based on the target STM32 family defined in your build flags (e.g. `-DSTM32F401xE`). Supported families are listed in `ltr390_driver.hpp`.

---

# 8. Example Usage (C++)

```cpp
Ltr390Driver ltr390_sensor(&hi2c1);

float debug_uv_index = 0.0f;
float debug_lux      = 0.0f;

Ltr390Status init_status = ltr390_sensor.init();
if (init_status != Ltr390Status::OK) {
    Error_Handler();
}

while (1)
{
    (void)ltr390_sensor.calculate_uv_index();
    (void)ltr390_sensor.calculate_lux();

    debug_uv_index = ltr390_sensor.get_uv_index();
    debug_lux      = ltr390_sensor.get_lux();

    BSP_LED_Toggle(LED_GREEN);
    HAL_Delay(500);
}
```
