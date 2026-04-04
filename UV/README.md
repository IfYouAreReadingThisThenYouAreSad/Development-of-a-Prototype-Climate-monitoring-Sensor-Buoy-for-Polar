# UV Module (STM32)

If you are looking for libraries for **Arduino or Raspberry Pi ** and information on **LTR390-UV Ultraviolet Sensor**, please refer to the official WaveShare Wiki documentation:

https://www.waveshare.com/wiki/UV_Sensor_(C)

---

# 1. Overview

This directory contains a driver for interfacing with the **Digital LTR390-UV Ultraviolet Sensor** over I2C using an STM32 microcontroller.

The LTR390 enables real-time measurement of:
- Ambout light using onboard Amboent light sensor (ALS) 
- Ultra-viloit light

This module is used on the buoy to monitor UV light across seasons to help monitor climate change.
---

# 2. Hardware

The system uses the:

**Digital LTR390-UV Ultraviolet Sensor**

### Product Link
https://thepihut.com/products/digital-ltr390-uv-ultraviolet-sensor

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/BatteryMonitor_Module/ina219-high-side-dc-current-sensor-breakout-26v-3-2a-max-stemma-qt-adafruit-ada904-28609974796483_1000x.jpg" width="400">
</p>

<p align="center"><b>Figure 1:</b> UV Sensor Module</p>

---

# 3. Hardware Connections

The LTR390 communicates via the **I2C interface**.

## STM32 ↔ LTR390 Connections

| INA219 Pin | STM32 / System | Description                  |
|------------|----------------|------------------------------|
| VCC        | 3.3V/5V           | Module power                 |
| GND        | GND            | Common ground                |
| SCL        | I2C_SCL        | I2C clock line              |
| SDA        | I2C_SDA        | I2C data line               |


---

## Example STM32F401 Pin Mapping

| STM32 Pin | Peripheral | Function  |
|----------|-----------|----------|
| PB6      | I2C1_SCL  | I2C clock |
| PB7      | I2C1_SDA  | I2C data  |

---

# 4. API

## Constructor

| Function | Description |
|----------|------------|
| `Ltr390Driver(I2C_HandleTypeDef *hi2c_)` | Initialises the driver with the STM32 I2C peripheral. Internal variables are reset during construction. |

---


## Important Conexpr class vairables

These are important vairiables that are held constant at run time and are currently set for this projects application, therefore it might be worth changing these before compling for different projects.
These function are also abstracted away from the user so they cant be accidently changed , these can be found in the private section of the class in the .hpp file
| Variable | Description |
|----------|------------|
| `WINDOW_FACTOR` |  compensates for UV loss through the buoy enclosure window. |
| `DATA_READY_TIMEOUT_MS` |  Time UV sensor has to be ready to be read again before TIMEOUT error , used in wait_data_ready() |
| `MEAS_RATE_DEFAULT` | MEAS_RATE = (resolution bits) | (rate bits) |
| `GAIN` | Sets gain value. |
| `COUNTS_PER_UVI` | Reference sensitivity constant. Value needs to be change if GAIN or MEAS_RATE_DEFAULT is change. Values come from datasheet |



## Public Functions

| Function | Description |
|----------|------------|
| `Ltr390Status init()` | Initialises the sensor by setting resolution, sampling rate and gain. |
| `Ltr390Status calculate_uv_index();` | read uv sensor and calculates the UV index. |
| `Ltr390Status calculate_lux();` | Returns ambient light sensor and calculates the lux. |
| `get_uv_index()` | Returns the uv index values from private class variable. |
| `get_lux()` | Returns the lux value from private class variable. |

---

# 5. Enums

## `Ltr390Status`

Used for error handling and debugging.

| Value | Description |
|------|------------|
| `Ltr390Status::OK` | Operation successful |
| `Ltr390Status::I2C_NO_ACKNOWLEDGMENT` | Wrong I2C address passed into constructor |
| `Ltr390Status::I2C_TRANSMIT_ERROR` | I2C communication failure on Transmitting to slave |
| `Ltr390Status::I2C_RECEIVE_ERROR` | I2C communication failure on Recieving to master |
| `Ltr390Status::TIMEOUT` | Timeout error when waiting for the sensor to be ready to read data from|
| `Ltr390Status::INVALID_ARGUMENT` | Invalid Window Factor Value |
| `Ltr390Status::RESOLUTION_SETUP_ERROR` | I2C error in setting up resolution of sensor |
| `Ltr390Status::GAIN_SETUP_ERROR` | I2C error in setting up gain of sensor |
| `Ltr390Status::UV_MODE_SWITCH_ERROR` | Failied to switch to reading on board UV sensor |
| `Ltr390Status::ALS_MODE_SWITCH_ERROR` | Failed to switch to reading on board ambient light sensor |


---

# 6. Code Flow

<p align="center">
<img src="https://github.com/IfYouAreReadingThisThenYouAreSad/Development-of-a-Prototype-Climate-monitoring-Sensor-Buoy-for-Polar/blob/main/BatteryMonitor_Module/INA219%20Diagram.jpeg" width="800">
</p>

<p align="center"><b>Figure 2:</b> INA219 Processing Pipeline</p>

## Processing Steps

1. **Initialisation (run once)**
   - Resolution and sampling 
   - Sets gain  
   - Changes sensor to UV mode  

2. **UV Measurement**
   - Put sensor into UV mode
   - Wait until sensor is ready to get data from
   - Reads raw UV count
   - Calculate UV index

3. **Lux Measurement**
   - Put sensor into ALS mode 
   - Wait until sensor is ready to get data from
   - reads raw ALS count
   - Calculate Lux

 

---

# 7. Dependencies

- STM32 HAL Library  
- STM32 I2C Peripheral  

---

# 8. Example Usage (C++)

```cpp
   Ltr390Driver ltr390_sensor(&hi2c1);
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
